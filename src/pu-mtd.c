/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2025 PHYTEC Messtechnik GmbH
 */

#define G_LOG_DOMAIN "partup-mtd"

#include <fcntl.h>
#include <glib.h>
#include <linux/blkpg.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include "pu-checksum.h"
#include "pu-error.h"
#include "pu-file.h"
#include "pu-flash.h"
#include "pu-hashtable.h"
#include "pu-utils.h"
#include "pu-mtd.h"

typedef struct _PuMtdInput {
    gchar *filename;
    gchar *md5sum;
    gchar *sha256sum;

    /* Internal members */
    gsize _size;
} PuMtdInput;
typedef struct _PuMtdPartition {
    gchar *name;
    gint64 size;
    gint64 offset;
    gboolean erase;
    gboolean expand;
    PuMtdInput *input;
} PuMtdPartition;

struct _PuMtd {
    PuFlash parent_instance;

    GList *partitions;
};

G_DEFINE_TYPE(PuMtd, pu_mtd, PU_TYPE_FLASH)

typedef struct _PuMtdPartitionInfo {
    guint devnum;
    gchar *name;
    gint64 offset;
} PuMtdPartitionInfo;
typedef struct _PuMtdPartitionEnumerator {
    GFile *dir;
    GFileEnumerator *dir_enum;
    gchar *sysfs_device_path;
    GRegex *regex_part_num;
} PuMtdPartitionEnumerator;

static void
pu_mtd_partition_info_free(PuMtdPartitionInfo *part_info)
{
    if (!part_info)
        return;

    g_free(part_info->name);
    g_free(part_info);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(PuMtdPartitionInfo, pu_mtd_partition_info_free)

static void
pu_mtd_partition_enumerator_free(PuMtdPartitionEnumerator *part_enum)
{
    if (!part_enum)
        return;

    g_file_enumerator_close(part_enum->dir_enum, NULL, NULL);
    g_object_unref(part_enum->dir_enum);
    g_object_unref(part_enum->dir);
    g_regex_unref(part_enum->regex_part_num);
    g_free(part_enum->sysfs_device_path);
    g_free(part_enum);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(PuMtdPartitionEnumerator, pu_mtd_partition_enumerator_free)

static PuMtdPartitionEnumerator *
pu_mtd_enumerate_partitions(const gchar *device_path,
                            GError **error)
{
    g_autofree gchar *sysfs_path = NULL;
    g_autofree gchar *regex_part_num = NULL;
    PuMtdPartitionEnumerator *part_enum = NULL;
    const gchar *file_attr = G_FILE_ATTRIBUTE_STANDARD_NAME;

    g_return_val_if_fail(device_path != NULL, NULL);
    g_return_val_if_fail(!g_str_equal(device_path, ""), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    part_enum = g_new0(PuMtdPartitionEnumerator, 1);

    part_enum->sysfs_device_path = g_build_filename("/sys/class/mtd",
                                                    g_path_get_basename(device_path),
                                                    NULL);
    part_enum->regex_part_num = g_regex_new("[0-9]+$", 0, 0, NULL);
    part_enum->dir = g_file_new_for_path(part_enum->sysfs_device_path);
    if (!part_enum->dir) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Failed creating file descriptor for '%s'", device_path);
        return NULL;
    }
    part_enum->dir_enum = g_file_enumerate_children(part_enum->dir, file_attr,
                                                    G_FILE_QUERY_INFO_NONE,
                                                    NULL, error);
    if (!part_enum->dir_enum) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Failed creating enumerator for '%s'", device_path);
        return NULL;
    }

    return g_steal_pointer(&part_enum);
}

static gboolean
pu_mtd_partition_enumerator_iterate(PuMtdPartitionEnumerator *part_enum,
                                    PuMtdPartitionInfo **part_info,
                                    GError **error)
{
    GFileInfo *child_info;
    g_autoptr(GMatchInfo) match_info = NULL;
    g_autoptr(PuMtdPartitionInfo) out_info = NULL;
    g_autofree gchar *part_path = NULL;
    g_autofree gchar *part_offset_path = NULL;
    g_autofree gchar *part_name_path = NULL;
    g_autofree gchar *offset_content = NULL;
    g_autofree gchar *device_path = NULL;
    GError *temp_error = NULL;

    g_return_val_if_fail(part_enum != NULL, FALSE);

    device_path = g_file_get_path(part_enum->dir);

    while (TRUE) {
        child_info = g_file_enumerator_next_file(part_enum->dir_enum, NULL, &temp_error);
        if (temp_error != NULL) {
            g_propagate_error(error, temp_error);
            return FALSE;
        }

        if (!child_info)
            break;

        part_path = g_build_filename(device_path, g_file_info_get_name(child_info), NULL);
        part_offset_path = g_build_filename(part_path, "offset", NULL);
        if (!g_file_test(part_offset_path, G_FILE_TEST_EXISTS))
            continue;

        out_info = g_new0(PuMtdPartitionInfo, 1);
        g_regex_match(part_enum->regex_part_num, part_path, 0, &match_info);
        if (!g_match_info_matches(match_info)) {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_INIT,
                        "Failed finding partition number for '%s'", part_path);
            return FALSE;
        }
        out_info->devnum = g_ascii_strtoll(g_match_info_fetch(match_info, 0), NULL, 10);

        if (!g_file_get_contents(part_offset_path, &offset_content, NULL, error)) {
            g_prefix_error(error, "Failed getting offset of partition %u of "
                           "device '%s': ", out_info->devnum, device_path);
            return FALSE;
        }
        out_info->offset = g_ascii_strtoll(offset_content, NULL, 10);

        part_name_path = g_build_filename(part_path, "name", NULL);
        if (!g_file_get_contents(part_name_path, &out_info->name, NULL, error)) {
            g_prefix_error(error, "Failed getting name of partition %u of "
                           "device '%s': ", out_info->devnum, device_path);
            return FALSE;
        }
        out_info->name = g_strstrip(out_info->name);
        break;
    }

    if (*part_info) {
        pu_mtd_partition_info_free(*part_info);
        *part_info = NULL;
    }
    *part_info = g_steal_pointer(&out_info);

    return TRUE;
}

static gboolean
pu_mtd_init_device(PuFlash *flash,
                   GError **error)
{
    g_autofree gchar *device_path = NULL;
    g_autofree gchar *device_offset_path = NULL;
    g_autofree gchar *sysfs_path = NULL;
    g_autoptr(PuMtdPartitionEnumerator) part_enum = NULL;
    g_autoptr(PuMtdPartitionInfo) part_info = NULL;

    g_object_get(flash,
                 "device-path", &device_path,
                 NULL);

    g_message("Initializing MTD");

    /* Check if MTD is actual device and not a partition */
    sysfs_path = g_build_filename("/sys/class/mtd", g_path_get_basename(device_path), NULL);
    device_offset_path = g_build_filename(sysfs_path, "offset", NULL);
    if (g_file_test(device_offset_path, G_FILE_TEST_EXISTS)) {
        g_set_error(error, PU_ERROR, PU_ERROR_FLASH_INIT,
                    "'%s' appears to be a partition, but only complete devices "
                    "can be written", device_path);
        return FALSE;
    }

    /* Delete device's old partitions */
    part_enum = pu_mtd_enumerate_partitions(device_path, error);
    if (!part_enum)
        return FALSE;

    while (TRUE) {
        g_autofree gchar *cmd = NULL;

        if (!pu_mtd_partition_enumerator_iterate(part_enum, &part_info, error)) {
            g_prefix_error(error, "Failed iterating partition enumerator: ");
            return FALSE;
        }

        if (!part_info)
            break;

        cmd = g_strdup_printf("mtdpart del %s %u", device_path, part_info->devnum);
        if (!pu_spawn_command_line_sync(cmd, error)) {
            g_prefix_error(error, "Failed deleting partition %u of device '%s'",
                           part_info->devnum, device_path);
            return FALSE;
        }
    }

    return TRUE;
}

static PuMtdPartition *
find_partition(PuMtd *self,
               const gchar *name,
               gint64 offset,
               GError **error)
{
    PuMtdPartition *part;
    gint64 acc_offset = 0;

    g_return_val_if_fail(self, NULL);
    g_return_val_if_fail(PU_IS_MTD(self), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    for (GList *p = self->partitions; p != NULL; p = p->next) {
        part = p->data;
        acc_offset += part->offset;
        g_debug("Checking partition '%s' at %" G_GINT64_FORMAT,
                part->name, acc_offset);
        if (g_str_equal(part->name, name) && acc_offset == offset) {
            g_debug("Found partition '%s' at acc_offset=%" G_GINT64_FORMAT,
                    name, acc_offset);
            return part;
        }
        acc_offset += part->size;
    }

    g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                "Couldn't find partition for name '%s' and offset %" G_GINT64_FORMAT,
                name, offset);
    return NULL;
}

static gboolean
pu_mtd_setup_layout(PuFlash *flash,
                    GError **error)
{
    PuMtd *self = PU_MTD(flash);
    g_autofree gchar *device_path = NULL;
    g_autofree gchar *device_size_path = NULL;
    g_autofree gchar *erase_size_path = NULL;
    g_autoptr(PuMtdPartitionEnumerator) part_enum = NULL;
    g_autoptr(PuMtdPartitionInfo) part_info = NULL;
    gint64 device_size = 0;
    gint64 erase_size = 0;
    gint64 acc_offset = 0;

    g_object_get(flash,
                 "device-path", &device_path,
                 NULL);

    g_message("Partitioning MTD");

    device_size_path = g_strdup_printf("/sys/class/mtd/%s/size",
                                       g_path_get_basename(device_path));
    if (!pu_file_read_int64(device_size_path, &device_size, error)) {
        g_prefix_error(error, "Failed reading device size");
        return FALSE;
    }

    erase_size_path = g_strdup_printf("/sys/class/mtd/%s/erasesize",
                                      g_path_get_basename(device_path));
    if (!pu_file_read_int64(erase_size_path, &erase_size, error)) {
        g_prefix_error(error, "Failed reading erase size");
        return FALSE;
    }

    /* Add partitions */
    for (GList *p = self->partitions; p != NULL; p = p->next) {
        g_autofree gchar *cmd = NULL;
        PuMtdPartition *part = p->data;

        acc_offset += part->offset;
        if (part->expand)
            part->size = device_size - acc_offset;
        g_debug("acc_offset=%" G_GINT64_FORMAT " "
                "part->offset=%" G_GINT64_FORMAT " "
                "part->size=%" G_GINT64_FORMAT,
                acc_offset, part->offset, part->size);

        if (acc_offset + part->size > device_size) {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_LAYOUT,
                        "Partition '%s' at offset %" G_GINT64_FORMAT " with "
                        "size %" G_GINT64_FORMAT " is too large for device '%s' "
                        "(%" G_GINT64_FORMAT ")",
                        part->name, acc_offset, part->size, device_path, device_size);
            return FALSE;
        }

        if (part->size % erase_size) {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_LAYOUT,
                        "Partition '%s' at offset %" G_GINT64_FORMAT " has "
                        "incompatible size %" G_GINT64_FORMAT " not ending on "
                        "erase/write block (size must be a multiple of "
                        "%" G_GINT64_FORMAT ")",
                        part->name, acc_offset, part->size, erase_size);
            return FALSE;
        }

        cmd = g_strdup_printf("mtdpart add %s \"%s\" %" G_GINT64_FORMAT " "
                              "%" G_GINT64_FORMAT,
                              device_path, part->name, acc_offset, part->size);
        if (!pu_spawn_command_line_sync(cmd, error)) {
            g_prefix_error(error, "Failed adding partition '%s' at offset "
                           "%" G_GINT64_FORMAT " for device '%s'",
                           part->name, acc_offset, device_path);
            return FALSE;
        }
        acc_offset += part->size;
    }

    /* Erase partitions' content */
    part_enum = pu_mtd_enumerate_partitions(device_path, error);
    if (!part_enum)
        return FALSE;

    while (TRUE) {
        g_autofree gchar *cmd = NULL;
        const PuMtdPartition *p = NULL;

        if (!pu_mtd_partition_enumerator_iterate(part_enum, &part_info, error)) {
            g_prefix_error(error, "Failed iterating partition enumerator: ");
            return FALSE;
        }
        if (!part_info)
            break;

        p = find_partition(self, part_info->name, part_info->offset, error);
        if (!p)
            return FALSE;
        if (!p->erase)
            continue;

        cmd = g_strdup_printf("flash_erase -q /dev/mtd%u 0 0", part_info->devnum);
        if (!pu_spawn_command_line_sync(cmd, error)) {
            g_prefix_error(error, "Failed erasing partition '%s'", part_info->name);
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
pu_mtd_write_data(PuFlash *flash,
                  GError **error)
{
    PuMtd *self = PU_MTD(flash);
    gboolean skip_checksums = FALSE;
    g_autofree gchar *device_path = NULL;
    g_autofree gchar *prefix = NULL;
    g_autoptr(PuMtdPartitionEnumerator) part_enum = NULL;
    g_autoptr(PuMtdPartitionInfo) part_info = NULL;

    g_object_get(flash,
                 "device-path", &device_path,
                 "prefix", &prefix,
                 "skip-checksums", &skip_checksums,
                 NULL);

    g_message("Writing data to MTD");

    /* Write input binary */
    part_enum = pu_mtd_enumerate_partitions(device_path, error);
    if (!part_enum)
        return FALSE;

    while (TRUE) {
        g_autofree gchar *cmd = NULL;
        g_autofree gchar *path = NULL;
        g_autofree gchar *sha256sum = NULL;
        g_autofree gchar *part_dev = NULL;
        const PuMtdPartition *p = NULL;

        if (!pu_mtd_partition_enumerator_iterate(part_enum, &part_info, error)) {
            g_prefix_error(error, "Failed iterating partition enumerator: ");
            return FALSE;
        }
        if (!part_info)
            break;

        p = find_partition(self, part_info->name, part_info->offset, error);
        if (!p)
            return FALSE;
        if (!p->input)
            continue;

        path = pu_path_from_filename(p->input->filename, prefix, error);
        if (!path) {
            g_prefix_error(error, "Failed parsing input filename for partition: ");
            return FALSE;
        }

        part_dev = g_strdup_printf("/dev/mtd%u", part_info->devnum);
        cmd = g_strdup_printf("flashcp %s %s", path, part_dev);
        if (!pu_spawn_command_line_sync(cmd, error)) {
            g_prefix_error(error, "Failed writing data to partition '%s': ",
                           part_info->name);
            return FALSE;
        }

        if (!g_str_equal(p->input->md5sum, "") && !skip_checksums) {
            g_debug("Checking MD5 sum of output data in '%s'", path);
            if (!pu_checksum_verify_raw(part_dev, 0, p->input->_size,
                                        p->input->md5sum, G_CHECKSUM_MD5, error))
                return FALSE;
        }
        if (!g_str_equal(p->input->sha256sum, "") && !skip_checksums) {
            g_debug("Checking SHA256 sum of output data in '%s'", path);
            if (!pu_checksum_verify_raw(part_dev, 0, p->input->_size,
                                        p->input->sha256sum, G_CHECKSUM_SHA256, error))
                return FALSE;
        }
    }

    g_message("MTD partitions need to be updated in bootloader and/or kernel!");

    return TRUE;
}

static void
pu_mtd_class_finalize(GObject *object)
{
    G_OBJECT_CLASS(pu_mtd_parent_class)->finalize(object);
}

static void
pu_mtd_class_init(PuMtdClass *class)
{
    PuFlashClass *flash_class = PU_FLASH_CLASS(class);
    GObjectClass *object_class = G_OBJECT_CLASS(class);

    flash_class->init_device = pu_mtd_init_device;
    flash_class->setup_layout = pu_mtd_setup_layout;
    flash_class->write_data = pu_mtd_write_data;

    object_class->finalize = pu_mtd_class_finalize;
}

static void
pu_mtd_init(G_GNUC_UNUSED PuMtd *self)
{
}

static gboolean
pu_mtd_parse_partitions(PuMtd *mtd,
                        GHashTable *root,
                        GError **error)
{
    PuConfigValue *value_partitions = g_hash_table_lookup(root, "partitions");
    GList *partitions;
    g_autofree gchar *path = NULL;
    g_autofree gchar *prefix = NULL;

    g_return_val_if_fail(mtd != NULL, FALSE);
    g_return_val_if_fail(root != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_object_get(mtd,
                 "prefix", &prefix,
                 NULL);

    if (!value_partitions) {
        g_debug("No entry 'partitions' found. Skipping...");
        return TRUE;
    }

    if (value_partitions->type != PU_CONFIG_VALUE_TYPE_SEQUENCE) {
        g_set_error(error, PU_ERROR, PU_ERROR_MTD_PARSE,
                    "'partitions' is not a sequence");
        return FALSE;
    }
    partitions = value_partitions->data.sequence;

    for (GList *p = partitions; p != NULL; p = p->next) {
        PuConfigValue *v = p->data;
        if (v->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
            g_set_error(error, PU_ERROR, PU_ERROR_MTD_PARSE,
                        "'partitions' does not contain sequence of mappings");
            return FALSE;
        }

        PuMtdPartition *part = g_new0(PuMtdPartition, 1);
        part->name = pu_hash_table_lookup_string(v->data.mapping, "name", NULL);
        part->size = pu_hash_table_lookup_bytes(v->data.mapping, "size", 0);
        part->offset = pu_hash_table_lookup_bytes(v->data.mapping, "offset", 0);
        part->erase = pu_hash_table_lookup_boolean(v->data.mapping, "erase", TRUE);
        part->expand = pu_hash_table_lookup_boolean(v->data.mapping, "expand", FALSE);
        if (!part->name) {
            g_set_error(error, PU_ERROR, PU_ERROR_MTD_PARSE,
                        "Partition is missing a name");
            return FALSE;
        }
        if (part->size == 0 && !part->expand) {
            g_set_error(error, PU_ERROR, PU_ERROR_MTD_PARSE,
                        "Partition is missing a size");
            return FALSE;
        }
        if (part->expand && p->next) {
            g_set_error(error, PU_ERROR, PU_ERROR_MTD_PARSE,
                        "Only the last partition can be expanded");
            return FALSE;
        }
        g_debug("Parsed partition: name=%s size=%" G_GINT64_FORMAT " "
                "offset=%" G_GINT64_FORMAT " erase=%s expand=%s",
                part->name, part->size, part->offset,
                part->erase ? "true" : "false", part->expand ? "true" : "false");
        PuConfigValue *value_input = g_hash_table_lookup(v->data.mapping, "input");
        if (value_input) {
            if (value_input->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
                g_set_error(error, PU_ERROR, PU_ERROR_MTD_PARSE,
                            "'input' of partition does not contain a mapping");
                return FALSE;
            }
            PuMtdInput *input = g_new0(PuMtdInput, 1);
            input->filename = pu_hash_table_lookup_string(
                    value_input->data.mapping, "filename", "");
            input->md5sum = pu_hash_table_lookup_string(
                    value_input->data.mapping, "md5sum", "");
            input->sha256sum = pu_hash_table_lookup_string(
                    value_input->data.mapping, "sha256sum", "");

            path = pu_path_from_filename(input->filename, prefix, error);
            if (path == NULL)
                return FALSE;

            input->_size = pu_file_get_size(path, error);
            if (!input->_size)
                return FALSE;

            part->input = input;
            g_debug("Parsed partition input: filename=%s md5sum=%s sha256sum=%s",
                    input->filename, input->md5sum, input->sha256sum);

            if ((gint64) input->_size >= part->size) {
                g_set_error(error, PU_ERROR, PU_ERROR_MTD_PARSE,
                            "Input file '%s' (%" G_GINT64_FORMAT " bytes) "
                            "exceeds partition size (%" G_GINT64_FORMAT " bytes)",
                            input->filename, input->_size, part->size);
                return FALSE;
            }
        }

        mtd->partitions = g_list_prepend(mtd->partitions, part);
    }

    mtd->partitions = g_list_reverse(mtd->partitions);

    return TRUE;
}

PuMtd *
pu_mtd_new(const gchar *device_path,
           PuConfig *config,
           const gchar *prefix,
           gboolean skip_checksums,
           GError **error)
{
    PuMtd *self;
    GHashTable *root;

    g_return_val_if_fail(device_path != NULL, NULL);
    g_return_val_if_fail(!g_str_equal(device_path, ""), NULL);
    g_return_val_if_fail(config != NULL, NULL);
    g_return_val_if_fail(PU_IS_CONFIG(config), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    self = g_object_new(PU_TYPE_MTD,
                        "device-path", device_path,
                        "config", config,
                        "prefix", prefix,
                        "skip-checksums", skip_checksums,
                        NULL);
    root = pu_config_get_root(config);

    if (!pu_mtd_parse_partitions(self, root, error))
        return NULL;

    return g_steal_pointer(&self);
}
