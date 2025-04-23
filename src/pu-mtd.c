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
    gchar *label;
    gint64 size;
    gint64 offset;
    gboolean erase;
    PuMtdInput *input;

    /* Internal members */
    guint _devnum;
} PuMtdPartition;

struct _PuMtd {
    PuFlash parent_instance;

    GList *partitions;
};

G_DEFINE_TYPE(PuMtd, pu_mtd, PU_TYPE_FLASH)

#if 0
static GList *
pu_mtd_get_partition_devnums(PuMtd *self)
{
    GList *devnums = NULL;
    g_autofree gchar *device_path = NULL;
    g_autofree gchar *sysfs_path = NULL;
    g_autoptr(GFile) dir = NULL;
    g_autoptr(GFileEnumerator) dir_enum = NULL;
    g_autoptr(GRegex) regex_part_num = NULL;

    g_object_get(PU_FLASH(self),
                 "device-path", &device_path,
                 NULL);

    sysfs_path = g_build_filename("/sys/class/mtd", g_path_get_basename(device_path), NULL);
    regex_part_num = g_regex_new("[0-9]+$", 0, 0, NULL);

    dir = g_file_new_for_path(sysfs_path);
    dir_enum = g_file_enumerate_children(dir, file_attr, G_FILE_QUERY_INFO_NONE, NULL, error);
    if (!dir_enum) {
        g_set_error(error, PU_ERROR, PU_ERROR_FLASH_INIT,
                    "Failed creating enumerator for '%s'", g_file_get_path(dir));
        return NULL;
    }

    while (TRUE) {
        GFileInfo *child_info;
        g_autofree gchar *part_path = NULL;
        g_autofree gchar *offset_path = NULL;
        g_autofree gchar *name_path = NULL;
        g_autofree gchar *name = NULL;
        g_autoptr(GMatchInfo) match_info = NULL;
        guint devnum;

        if (!g_file_enumerator_iterate(dir_enum, &child_info, NULL, NULL, error)) {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_INIT,
                        "Failed iterating file enumerator for '%s'",
                        g_file_get_path(dir));
            return NULL;
        }

        if (!child_info)
            break;

        part_path = g_build_filename(g_file_get_path(dir),
                                     g_file_info_get_name(child_info), NULL);
        offset_path = g_build_filename(part_path, "offset", NULL);
        if (!g_file_test(offset_path, G_FILE_TEST_EXISTS))
            continue;

        g_regex_match(regex_part_num, part_path, 0, &match_info);
        if (!g_match_info_matches(match_info)) {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_INIT,
                        "Failed finding partition number for '%s'", part_path);
            g_list_free(g_steal_pointer(&devnums));
            return NULL;
        }
        devnum = g_ascii_strtoll(g_match_info_fetch(match_info, 0));
        devnums = g_list_prepend(devnums, GINT_TO_POINTER(devnum));

        name_path = g_build_filename(part_path, "name", NULL);
        if (!g_file_get_contents(name_path, &name, NULL, error)) {
            g_prefix_error(error, "Failed getting name of partition %u of "
                           "device '%s'", devnum, device_path);
            return NULL;
        }

        cmd = g_strdup_printf("mtdpart del %s %ld", device_path, devnum);
        if (!pu_spawn_command_line_sync(cmd, error)) {
            g_prefix_error(error, "Failed deleting partition %ld of device '%s'", d->data);
            g_list_free(g_steal_pointer(&devnums));
            return FALSE;
        }

        for (GList *p = self->partitions; p != NULL; p = p->next) {
            PuMtdPartition *part = p->data;
            if (g_str_equal(part->name, name))
                part->_devnum = devnum;
        }
    }

    return g_steal_pointer(&devnums);
}
#endif

static gboolean
pu_mtd_init_device(PuFlash *flash,
                   GError **error)
{
    //PuMtd *self = PU_MTD(flash);
    g_autofree gchar *device_path = NULL;
    g_autofree gchar *offset_path = NULL;
    g_autofree gchar *device_offset_path = NULL;
    g_autofree gchar *partitions = NULL;
    g_autofree gchar *sysfs_path = NULL;
    g_autoptr(GFileEnumerator) dir_enum = NULL;
    g_autoptr(GFile) dir = NULL;
    g_autoptr(GRegex) regex_part_num = NULL;
    const gchar *file_attr = G_FILE_ATTRIBUTE_STANDARD_NAME;

    g_object_get(flash,
                 "device-path", &device_path,
                 NULL);

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
    regex_part_num = g_regex_new("[0-9]+$", 0, 0, NULL);

    dir = g_file_new_for_path(sysfs_path);
    dir_enum = g_file_enumerate_children(dir, file_attr, G_FILE_QUERY_INFO_NONE, NULL, error);
    if (!dir_enum) {
        g_set_error(error, PU_ERROR, PU_ERROR_FLASH_INIT,
                    "Failed creating enumerator for '%s'", g_file_get_path(dir));
        return FALSE;
    }

    while (TRUE) {
        GFileInfo *child_info;
        guint devnum;
        g_autofree gchar *part_path = NULL;
        g_autofree gchar *cmd = NULL;
        g_autoptr(GMatchInfo) match_info = NULL;

        if (!g_file_enumerator_iterate(dir_enum, &child_info, NULL, NULL, error)) {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_INIT,
                        "Failed iterating file enumerator for '%s'",
                        g_file_get_path(dir));
            return FALSE;
        }

        if (!child_info)
            break;

        part_path = g_build_filename(g_file_get_path(dir),
                                     g_file_info_get_name(child_info), NULL);
        g_free(offset_path);
        offset_path = g_build_filename(part_path, "offset", NULL);
        if (!g_file_test(offset_path, G_FILE_TEST_EXISTS))
            continue;

        g_regex_match(regex_part_num, part_path, 0, &match_info);
        if (!g_match_info_matches(match_info)) {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_INIT,
                        "Failed finding partition number for '%s'", part_path);
            return FALSE;
        }

        devnum = g_ascii_strtoll(g_match_info_fetch(match_info, 0), NULL, 10);
        cmd = g_strdup_printf("mtdpart del %s %u", device_path, devnum);
        if (!pu_spawn_command_line_sync(cmd, error)) {
            g_prefix_error(error, "Failed deleting partition %u of device '%s'",
                           devnum, device_path);
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
pu_mtd_setup_layout(PuFlash *flash,
                    GError **error)
{
    /* TODO:
     * 1. Add partitions as specified in the config. See "mtdpart add".
     * 2. Erase each partition's content. See "flash_erase".
     */
    PuMtd *self = PU_MTD(flash);
    //GList *devnums = NULL;
    g_autofree gchar *device_path = NULL;

    g_object_get(flash,
                 "device-path", &device_path,
                 NULL);

    /* Add partitions */
    for (GList *p = self->partitions; p != NULL; p = p->next) {
        g_autofree gchar *cmd = NULL;
        PuMtdPartition *part = p->data;

        cmd = g_strdup_printf("mtdpart add %s \"%s\" %ld %ld", device_path,
                              part->label, part->offset, part->size);
        if (!pu_spawn_command_line_sync(cmd, error)) {
            g_prefix_error(error, "Failed adding partition '%s' at offset %ld "
                           "for device '%s'", part->label, part->offset, device_path);
            return FALSE;
        }
    }

#if 0
    devnums = pu_mtd_get_partition_devnums(device_path);

    for (GList *d = devnums; d != NULL; d = d->next) {
        g_autofree gchar *cmd = NULL;
        gint64 devnum = d->data;

        /* Erase partitions */
        cmd = g_strdup_printf("flash_erase -q \"/dev/mtd/by-name/%s\" 0 0", part->label);
        if (!pu_spawn_command_line_sync(cmd, error)) {
            g_prefix_error(error, "Failed erasing partition '%s' at offset %ld "
                           "for device '%s'", part->label, part->offset, device_path);
            return FALSE;
        }
    }
#endif

    return TRUE;
}

static gboolean
pu_mtd_write_data(PuFlash *flash,
                  GError **error)
{
    /* TODO:
     * 1. Check if input binary fits target partition.
     * 2. Get length and checksum of input binary.
     * 3. Write input binary byte-wise to target partition.
     * 4. Verify checksum of written data in target partition with the one
     *    produced earlier.
     */
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
        part->label = pu_hash_table_lookup_string(v->data.mapping, "label", NULL);
        part->size = pu_hash_table_lookup_bytes(v->data.mapping, "size", 0);
        part->offset = pu_hash_table_lookup_bytes(v->data.mapping, "offset", 0);
        part->erase = pu_hash_table_lookup_boolean(v->data.mapping, "erase", TRUE);
        if (part->size == 0) {
            g_set_error(error, PU_ERROR, PU_ERROR_MTD_PARSE,
                        "Partition is missing a size");
            return FALSE;
        }
        PuConfigValue *value_input = g_hash_table_lookup(v->data.mapping, "input");
        if (value_input) {
            if (value_input->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
                g_set_error(error, PU_ERROR, PU_ERROR_MTD_PARSE,
                            "'input' of partition does not contain a mapping");
                return FALSE;
            }
            PuMtdInput *input = g_new0(PuMtdInput, 1);
            input->filename = pu_hash_table_lookup_string(value_input->data.mapping, "filename", "");
            input->md5sum = pu_hash_table_lookup_string(value_input->data.mapping, "md5sum", "");
            input->sha256sum = pu_hash_table_lookup_string(value_input->data.mapping, "sha256sum", "");

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
                            "Input file '%s' (%ld bytes) exceeds partition size (%ld bytes)",
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
