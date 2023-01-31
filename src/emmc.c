/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <parted/parted.h>
#include <glib/gstdio.h>
#include "checksum.h"
#include "error.h"
#include "hashtable.h"
#include "mount.h"
#include "utils.h"
#include "emmc.h"

#define PARTITION_TABLE_SIZE_MSDOS 1
#define PARTITION_TABLE_SIZE_GPT   34

typedef struct _PuEmmcInput {
    gchar *uri;
    gchar *md5sum;
    gchar *sha256sum;
} PuEmmcInput;
typedef struct _PuEmmcPartition {
    gchar *label;
    gchar *partuuid;
    PedPartitionType type;
    gchar *filesystem;
    PedSector size;
    PedSector offset;
    PedSector block_size;
    gboolean expand;
    GList *flags;
    GList *input;
} PuEmmcPartition;
typedef struct _PuEmmcBinary {
    PedSector input_offset;
    PedSector output_offset;
    PuEmmcInput *input;
} PuEmmcBinary;
typedef struct _PuEmmcBootPartitions {
    gboolean enable;
    PedSector input_offset;
    PedSector output_offset;
    PuEmmcInput *input;
} PuEmmcBootPartitions;
typedef struct _PuEmmcClean {
    PedSector size;
    PedSector offset;
} PuEmmcClean;

struct _PuEmmc {
    PuFlash parent_instance;

    PedSector expanded_part_size;
    guint num_expanded_parts;
    guint num_logical_parts;

    PedDevice *device;
    PedDisk *disk;

    PedDiskType *disktype;
    GList *partitions;
    GList *clean;
    GList *raw;
    PuEmmcBootPartitions *emmc_boot_partitions;
};

G_DEFINE_TYPE(PuEmmc, pu_emmc, PU_TYPE_FLASH)

static gboolean
emmc_create_partition(PuEmmc *self,
                      PuEmmcPartition *partition,
                      PedSector start,
                      GError **error)
{
    /* TODO: Rename to more distinct name to not collision with PuEmmcPartition */
    PedPartition *part;
    PedFileSystemType *fstype = NULL;
    PedConstraint *constraint;
    PedSector length = partition->size - 1;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (g_strcmp0(partition->filesystem, "") > 0) {
        fstype = ped_file_system_type_get(partition->filesystem);
        if (fstype == NULL) {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_LAYOUT,
                        "Failed to recognize filesystem type '%s'",
                        partition->filesystem);
            return FALSE;
        }
    }

    if (partition->type == PED_PARTITION_LOGICAL) {
        if (!ped_disk_type_check_feature(self->disk->type, PED_DISK_TYPE_EXTENDED)) {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_LAYOUT,
                        "Logical partitions are not supported on this disk");
            return FALSE;
        }
        if (ped_disk_extended_partition(self->disk) == NULL) {
            g_debug("Creating extended partition at %lld", start);
            PedPartition *extpart;
            gint ret;

            extpart = ped_partition_new(self->disk, PED_PARTITION_EXTENDED,
                                        NULL, start, self->device->length - 1);
            constraint = ped_constraint_exact(&extpart->geom);
            ret = ped_disk_add_partition(self->disk, extpart, constraint);
            ped_constraint_destroy(constraint);

            if (!ret) {
                g_set_error(error, PU_ERROR, PU_ERROR_FLASH_LAYOUT,
                            "Failed adding extended partition");
                return FALSE;
            }
        }
        /* EBR needs at least 2 sectors in front of a logical partition */
        start += 2;
        length -= 2;
        g_debug("logical partition: start=%lld, length=%lld", start, length);
    }

    part = ped_partition_new(self->disk, partition->type, fstype,
                             start, start + length);
    if (part == NULL) {
        g_set_error(error, PU_ERROR, PU_ERROR_FLASH_LAYOUT,
                    "Failed creating partition");
        return FALSE;
    }

    constraint = ped_constraint_exact(&part->geom);
    if (constraint == NULL) {
        g_set_error(error, PU_ERROR, PU_ERROR_FLASH_LAYOUT,
                    "Failed retrieving constraint from partition geometry");
        return FALSE;
    }

    if (ped_disk_type_check_feature(part->disk->type, PED_DISK_TYPE_PARTITION_NAME)) {
        ped_partition_set_name(part, partition->label);
    }

    for (GList *f = partition->flags; f != NULL; f = f->next) {
        if (!ped_partition_set_flag(part, GPOINTER_TO_INT(f->data), 1)) {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_LAYOUT,
                        "Failed setting partition flag '%s'",
                        ped_partition_flag_get_name(GPOINTER_TO_INT(f->data)));
            return FALSE;
        }
    }

    if (!ped_disk_add_partition(self->disk, part, constraint)) {
        g_set_error(error, PU_ERROR, PU_ERROR_FLASH_LAYOUT,
                    "Failed adding partition to disk");
        return FALSE;
    }

    ped_constraint_destroy(constraint);

    return TRUE;
}

static gboolean
pu_emmc_init_device(PuFlash *flash,
                    GError **error)
{
    PuEmmc *self = PU_EMMC(flash);
    PedDisk *newdisk;

    g_debug(G_STRFUNC);

    g_return_val_if_fail(flash != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    newdisk = ped_disk_new_fresh(self->device, self->disktype);
    if (newdisk == NULL) {
        g_set_error(error, PU_ERROR, PU_ERROR_FLASH_INIT,
                    "Failed creating new disk");
        return FALSE;
    }

    if (self->disk) {
        ped_disk_destroy(self->disk);
    }
    self->disk = newdisk;
    ped_disk_commit(self->disk);

    g_debug("%s: Finished", G_STRFUNC);

    return TRUE;
}

static gboolean
pu_emmc_setup_layout(PuFlash *flash,
                     GError **error)
{
    PuEmmc *self = PU_EMMC(flash);
    PedSector part_start = 0;

    g_debug(G_STRFUNC);

    g_return_val_if_fail(flash != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    for (GList *p = self->partitions; p != NULL; p = p->next) {
        PuEmmcPartition *part = p->data;

        if (part->expand) {
            part->size = self->expanded_part_size;
        }

        if (part->block_size > 0) {
            part->size -= part->size % part->block_size;
        }

        /* Increase size of logical partitions by 2 sectors for EBR.
         * The 2 sectors are removed during partition creation. */
        if (part->type == PED_PARTITION_LOGICAL) {
            part->size += 2;
        }

        g_debug("%s: type=%d filesystem=%s start=%lld size=%lld offset=%lld "
                "block-size=%lld expand=%s",
                G_STRFUNC, part->type, part->filesystem, part_start, part->size,
                part->offset, part->block_size, part->expand ? "true" : "false");

        if (!emmc_create_partition(self, part, part_start + part->offset, error)) {
            return FALSE;
        }
        part_start += part->size + part->offset;
    }

    ped_disk_commit(self->disk);

    return TRUE;
}

static gboolean
pu_emmc_write_data(PuFlash *flash,
                   GError **error)
{
    PuEmmc *self = PU_EMMC(flash);
    guint i = 0;
    gboolean first_logical_part = FALSE;
    gboolean skip_checksums = FALSE;
    g_autofree gchar *part_path = NULL;
    g_autofree gchar *part_mount = NULL;
    g_autofree gchar *prefix = NULL;

    g_debug(G_STRFUNC);

    g_return_val_if_fail(flash != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_object_get(flash,
                 "prefix", &prefix,
                 "skip-checksums", &skip_checksums,
                 NULL);

    for (GList *p = self->partitions; p != NULL; p = p->next) {
        PuEmmcPartition *part = p->data;
        if (part->type == PED_PARTITION_LOGICAL && first_logical_part == FALSE) {
            first_logical_part = TRUE;
            i = 5;
        } else {
            i++;
        }

        if (g_strcmp0(part->partuuid, "") > 0) {
            if (g_str_equal(self->disktype->name, "gpt")) {
                if (!pu_partition_set_partuuid(self->device->path, i, part->partuuid, error))
                    return FALSE;
            } else {
                g_warning("Setting PARTUUID is only supported on GPT partitioned devices");
            }
        }

        part_path = pu_device_get_partition_path(self->device->path, i, error);
        if (part_path == NULL)
            return FALSE;

        g_debug("Creating filesystem %s on %s", part->filesystem, part_path);

        if (!pu_make_filesystem(part_path, part->filesystem, error))
            return FALSE;

        if (!part->input) {
            g_debug("No input specified. Skipping %s", part_path);
            continue;
        }

        part_mount = pu_create_mount_point(g_strdup_printf("p%u", i), error);
        if (part_mount == NULL)
            return FALSE;

        for (GList *i = part->input; i != NULL; i = i->next) {
            PuEmmcInput *input = i->data;
            g_autofree gchar *path = NULL;

            path = pu_path_from_uri(input->uri, prefix, error);
            if (path == NULL) {
                g_prefix_error(error, "Failed parsing input URI for partition: ");
                return FALSE;
            }

            if (!g_str_equal(input->md5sum, "") && !skip_checksums) {
                g_debug("Checking MD5 sum of input file '%s'", path);
                if (!pu_checksum_verify_file(path, input->md5sum,
                                             G_CHECKSUM_MD5, error))
                    return FALSE;
            }
            if (!g_str_equal(input->sha256sum, "") && !skip_checksums) {
                g_debug("Checking SHA256 sum of input file '%s'", path);
                if (!pu_checksum_verify_file(path, input->sha256sum,
                                             G_CHECKSUM_SHA256, error))
                    return FALSE;
            }

            if (g_regex_match_simple(".tar", path, G_REGEX_CASELESS, 0)) {
                g_debug("Extracting '%s' to '%s'", path, part_mount);
                if (!pu_mount(part_path, part_mount, error))
                    return FALSE;
                if (!pu_archive_extract(path, part_mount, error))
                    return FALSE;
                if (!pu_umount(part_mount, error))
                    return FALSE;
            } else if (g_regex_match_simple(".ext[234]$", path, 0, 0)) {
                g_debug("Writing '%s' to '%s'", path, part_mount);
                if (!pu_write_raw(path, part_path, self->device, 0, 0, 0, error))
                    return FALSE;
                if (!pu_resize_filesystem(part_path, error))
                    return FALSE;
            } else {
                g_debug("Copying '%s' to '%s'", path, part_mount);
                if (!pu_mount(part_path, part_mount, error))
                    return FALSE;
                if (!pu_file_copy(path, part_mount, error))
                    return FALSE;
                if (!pu_umount(part_mount, error))
                    return FALSE;
            }
        }
        g_rmdir(part_mount);
    }

    for (GList *c = self->clean; c != NULL; c = c->next) {
        PuEmmcClean *clean = c->data;

        if (clean->size == 0) {
            g_warning("Size 0 specified. Skipping cleaning at %lld", clean->offset);
            continue;
        }

        if (!pu_write_raw("/dev/zero", self->device->path, self->device, 0,
                          clean->offset, clean->size, error)) {
            return FALSE;
        }
    }

    for (GList *b = self->raw; b != NULL; b = b->next) {
        PuEmmcBinary *bin = b->data;
        PuEmmcInput *input = bin->input;
        g_autofree gchar *path = NULL;

        path = pu_path_from_uri(input->uri, prefix, error);
        if (path == NULL) {
            g_prefix_error(error, "Failed parsing input URI for binary: ");
            return FALSE;
        }

        if (g_str_equal(path, "")) {
            g_warning("No input specified for binary");
            continue;
        }

        if (!g_str_equal(input->md5sum, "") && !skip_checksums) {
            g_debug("Checking MD5 sum of input file '%s'", path);
            if (!pu_checksum_verify_file(path, input->md5sum,
                                         G_CHECKSUM_MD5, error))
                return FALSE;
        }
        if (!g_str_equal(input->sha256sum, "") && !skip_checksums) {
            g_debug("Checking SHA256 sum of input file '%s'", path);
            if (!pu_checksum_verify_file(path, input->sha256sum,
                                         G_CHECKSUM_SHA256, error))
                return FALSE;
        }

        if (!pu_write_raw(path, self->device->path, self->device,
                          bin->input_offset, bin->output_offset, 0, error))
            return FALSE;
    }

    if (self->emmc_boot_partitions) {
        PuEmmcInput *input = self->emmc_boot_partitions->input;
        g_autofree gchar *path = NULL;

        path = pu_path_from_uri(input->uri, prefix, error);
        if (path == NULL) {
            g_prefix_error(error, "Failed parsing input URI for eMMC boot partition: ");
            return FALSE;
        }

        if (g_str_equal(path, "")) {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_DATA,
                        "No input specified for eMMC boot partition");
            return FALSE;
        }

        if (!g_str_equal(input->md5sum, "") && !skip_checksums) {
            g_debug("Checking MD5 sum of input file '%s'", path);
            if (!pu_checksum_verify_file(path, input->md5sum,
                                         G_CHECKSUM_MD5, error))
                return FALSE;
        }
        if (!g_str_equal(input->sha256sum, "") && !skip_checksums) {
            g_debug("Checking SHA256 sum of input file '%s'", path);
            if (!pu_checksum_verify_file(path, input->sha256sum,
                                         G_CHECKSUM_SHA256, error))
                return FALSE;
        }

        if (!pu_write_raw_bootpart(path, self->device, 0,
                                   self->emmc_boot_partitions->input_offset,
                                   self->emmc_boot_partitions->output_offset,
                                   error))
            return FALSE;

        if (!pu_write_raw_bootpart(path, self->device, 1,
                                   self->emmc_boot_partitions->input_offset,
                                   self->emmc_boot_partitions->output_offset,
                                   error))
            return FALSE;

        if (!pu_bootpart_enable(self->device->path,
                                self->emmc_boot_partitions->enable, error))
            return FALSE;
    }

    g_debug("%s: Finished", G_STRFUNC);

    return TRUE;
}

static void
pu_emmc_class_finalize(GObject *object)
{
    PuEmmc *emmc = PU_EMMC(object);

    for (GList *p = emmc->partitions; p != NULL; p = p->next) {
        PuEmmcPartition *part = p->data;
        g_free(part->label);
        g_free(part->partuuid);
        g_free(part->filesystem);
        g_list_free(g_steal_pointer(&part->flags));
        for (GList *i = part->input; i != NULL; i = i->next) {
            PuEmmcInput *in = i->data;
            g_free(in->uri);
            g_free(in->md5sum);
            g_free(in->sha256sum);
            g_free(in);
        }
        g_list_free(g_steal_pointer(&part->input));
    }
    g_list_free(g_steal_pointer(&emmc->partitions));

    g_list_free(g_steal_pointer(&emmc->clean));

    for (GList *b = emmc->raw; b != NULL; b = b->next) {
        PuEmmcBinary *bin = b->data;
        g_free(bin->input->uri);
        g_free(bin->input->md5sum);
        g_free(bin->input->sha256sum);
        g_free(bin->input);
    }
    g_list_free(g_steal_pointer(&emmc->raw));

    if (emmc->emmc_boot_partitions) {
        PuEmmcInput *input = emmc->emmc_boot_partitions->input;
        g_free(input->uri);
        g_free(input->md5sum);
        g_free(input->sha256sum);
        g_free(input);
        g_free(emmc->emmc_boot_partitions);
    }

    if (emmc->disk)
        ped_disk_destroy(emmc->disk);
    if (emmc->device)
        ped_device_destroy(emmc->device);

    G_OBJECT_CLASS(pu_emmc_parent_class)->finalize(object);
}

static void
pu_emmc_class_init(PuEmmcClass *class)
{
    PuFlashClass *flash_class = PU_FLASH_CLASS(class);
    GObjectClass *object_class = G_OBJECT_CLASS(class);

    flash_class->init_device = pu_emmc_init_device;
    flash_class->setup_layout = pu_emmc_setup_layout;
    flash_class->write_data = pu_emmc_write_data;

    object_class->finalize = pu_emmc_class_finalize;
}

static void
pu_emmc_init(G_GNUC_UNUSED PuEmmc *self)
{
}

static gboolean
pu_emmc_parse_emmc_bootpart(PuEmmc *emmc,
                            GHashTable *root,
                            GError **error)
{
    PuConfigValue *value_bootpart = g_hash_table_lookup(root, "emmc-boot-partitions");

    g_return_val_if_fail(emmc != NULL, FALSE);
    g_return_val_if_fail(root != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!value_bootpart) {
        g_debug("No entry 'emmc-boot-partitions' found. Skipping...");
        return TRUE;
    }

    if (value_bootpart->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
        g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                    "'emmc-boot-partitions' is not a valid YAML mapping");
        return FALSE;
    }

    GHashTable *bootpart = value_bootpart->data.mapping;
    emmc->emmc_boot_partitions = g_new0(PuEmmcBootPartitions, 1);
    emmc->emmc_boot_partitions->enable =
        pu_hash_table_lookup_boolean(bootpart, "enable", FALSE);
    emmc->emmc_boot_partitions->input_offset =
        pu_hash_table_lookup_sector(bootpart, emmc->device, "input-offset", 0);
    emmc->emmc_boot_partitions->output_offset =
        pu_hash_table_lookup_sector(bootpart, emmc->device, "output-offset", 0);

    g_debug("Parsed bootpart: enable=%s input-offset=%lld output-offset=%lld",
            emmc->emmc_boot_partitions->enable ? "true" : "false",
            emmc->emmc_boot_partitions->input_offset,
            emmc->emmc_boot_partitions->output_offset);

    PuConfigValue *value_input = g_hash_table_lookup(bootpart, "input");
    if (value_input == NULL) {
        g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                    "No input specified for 'emmc-boot-partitions'");
        return FALSE;
    }
    if (value_input->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
        g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                    "'input' of binary does not contain a mapping");
        return FALSE;
    }
    PuEmmcInput *input = g_new0(PuEmmcInput, 1);
    input->uri = pu_hash_table_lookup_string(value_input->data.mapping, "uri", "");
    input->md5sum = pu_hash_table_lookup_string(value_input->data.mapping, "md5sum", "");
    input->sha256sum = pu_hash_table_lookup_string(value_input->data.mapping, "sha256sum", "");
    emmc->emmc_boot_partitions->input = input;

    g_debug("Parsed bootpart input: uri=%s md5sum=%s sha256sum=%s",
            input->uri, input->md5sum, input->sha256sum);

    return TRUE;
}

static gboolean
pu_emmc_parse_clean(PuEmmc *emmc,
                    GHashTable *root,
                    GError **error)
{
    PuConfigValue *value_clean = g_hash_table_lookup(root, "clean");
    GList *clean;

    g_return_val_if_fail(emmc != NULL, FALSE);
    g_return_val_if_fail(root != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!value_clean) {
        g_debug("No entry 'clean' found. Skipping...");
        return TRUE;
    }

    if (value_clean->type != PU_CONFIG_VALUE_TYPE_SEQUENCE) {
        g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                    "'clean' is not a sequence");
        return FALSE;
    }
    clean = value_clean->data.sequence;

    for (GList *c = clean; c != NULL; c = c->next) {
        PuConfigValue *v = c->data;
        PuEmmcClean *remove = g_new0(PuEmmcClean, 1);
        remove->offset = pu_hash_table_lookup_sector(v->data.mapping, emmc->device,
                                                     "offset", 0);
        remove->size = pu_hash_table_lookup_sector(v->data.mapping, emmc->device,
                                                   "size", 0);
        emmc->clean = g_list_prepend(emmc->clean, remove);
    }

    emmc->clean = g_list_reverse(emmc->clean);

    return TRUE;
}

static gboolean
pu_emmc_parse_raw(PuEmmc *emmc,
                  GHashTable *root,
                  GError **error)
{
    PuConfigValue *value_raw = g_hash_table_lookup(root, "raw");
    GList *raw;

    g_return_val_if_fail(emmc != NULL, FALSE);
    g_return_val_if_fail(root != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!value_raw) {
        g_debug("No entry 'raw' found. Skipping...");
        return TRUE;
    }

    if (value_raw->type != PU_CONFIG_VALUE_TYPE_SEQUENCE) {
        g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                    "'raw' is not a sequence");
        return FALSE;
    }
    raw = value_raw->data.sequence;

    for (GList *b = raw; b != NULL; b = b->next) {
        PuConfigValue *v = b->data;
        PuEmmcBinary *bin = g_new0(PuEmmcBinary, 1);
        bin->input_offset = pu_hash_table_lookup_sector(v->data.mapping, emmc->device,
                                                        "input-offset", 0);
        bin->output_offset = pu_hash_table_lookup_sector(v->data.mapping, emmc->device,
                                                         "output-offset", 0);
        PuConfigValue *value_input = g_hash_table_lookup(v->data.mapping, "input");
        if (value_input == NULL) {
            g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                        "No input specified for 'raw'");
            return FALSE;
        }
        if (value_input->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
            g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                        "'input' of binary does not contain a mapping");
            return FALSE;
        }
        PuEmmcInput *input = g_new0(PuEmmcInput, 1);
        input->uri = pu_hash_table_lookup_string(value_input->data.mapping, "uri", "");
        input->md5sum = pu_hash_table_lookup_string(value_input->data.mapping, "md5sum", "");
        input->sha256sum = pu_hash_table_lookup_string(value_input->data.mapping, "sha256sum", "");
        bin->input = input;
        g_debug("Parsed raw input: uri=%s md5sum=%s sha256sum=%s",
                input->uri, input->md5sum, input->sha256sum);

        emmc->raw = g_list_prepend(emmc->raw, bin);
    }

    emmc->raw = g_list_reverse(emmc->raw);

    return TRUE;
}

static gboolean
pu_emmc_parse_partitions(PuEmmc *emmc,
                         GHashTable *root,
                         GError **error)
{
    PuConfigValue *value_partitions = g_hash_table_lookup(root, "partitions");
    GList *partitions;
    PedSector fixed_parts_size = 0;
    gboolean first_partition = TRUE;

    g_return_val_if_fail(emmc != NULL, FALSE);
    g_return_val_if_fail(root != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    emmc->num_expanded_parts = 0;

    if (!value_partitions) {
        g_debug("No entry 'partitions' found. Skipping...");
        return TRUE;
    }

    if (value_partitions->type != PU_CONFIG_VALUE_TYPE_SEQUENCE) {
        g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                    "'partitions' is not a sequence");
        return FALSE;
    }
    partitions = value_partitions->data.sequence;

    for (GList *p = partitions; p != NULL; p = p->next) {
        PuConfigValue *v = p->data;
        if (v->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
            g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                        "'partitions' does not contain sequence of mappings");
            return FALSE;
        }

        PuEmmcPartition *part = g_new0(PuEmmcPartition, 1);
        part->label = pu_hash_table_lookup_string(v->data.mapping, "label", "");
        part->partuuid = pu_hash_table_lookup_string(v->data.mapping, "partuuid", "");
        part->filesystem = pu_hash_table_lookup_string(v->data.mapping, "filesystem", "fat32");
        part->size = pu_hash_table_lookup_sector(v->data.mapping, emmc->device, "size", 0);
        part->offset = pu_hash_table_lookup_sector(v->data.mapping, emmc->device, "offset", 0);
        part->block_size = pu_hash_table_lookup_sector(v->data.mapping, emmc->device, "block-size", 2);
        part->expand = pu_hash_table_lookup_boolean(v->data.mapping, "expand", FALSE);

        g_autofree gchar *type_str = pu_hash_table_lookup_string(v->data.mapping, "type", "primary");
        if (g_str_equal(type_str, "primary")) {
            part->type = PED_PARTITION_NORMAL;
        } else if (g_str_equal(type_str, "logical")) {
            part->type = PED_PARTITION_LOGICAL;
        } else {
            g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                        "Partition of invalid type '%s' specified", type_str);
            return FALSE;
        }

        if (first_partition) {
            first_partition = FALSE;
            PedSector min_offset = 1;

            if (g_str_equal(emmc->disktype->name, "msdos"))
                min_offset = PARTITION_TABLE_SIZE_MSDOS;
            else if (g_str_equal(emmc->disktype->name, "gpt"))
                min_offset = PARTITION_TABLE_SIZE_GPT;

            if (part->offset < min_offset && part->offset > 0) {
                g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                            "Offset of first partition too small and would "
                            "override partition table");
                return FALSE;
            }

            if (part->offset == 0) {
                g_warning("No offset specified for first partition (using "
                          "default of %lld sectors)", min_offset);
                part->offset = min_offset;
            }
        }

        g_debug("Parsed partition: label=%s filesystem=%s type=%s size=%lld "
                "offset=%lld block-size=%lld expand=%s",
                part->label, part->filesystem, type_str, part->size, part->offset,
                part->block_size, part->expand ? "true" : "false");

        GList *flag_list = pu_hash_table_lookup_list(v->data.mapping, "flags", NULL);
        if (flag_list != NULL) {
            for (GList *f = flag_list; f != NULL; f = f->next) {
                PuConfigValue *fv = f->data;
                if (fv->type != PU_CONFIG_VALUE_TYPE_STRING) {
                    g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                                "'flags' does not contain a sequence of strings");
                    return FALSE;
                }

                PedPartitionFlag flag = ped_partition_flag_get_by_name(fv->data.string);
                if (!flag) {
                    g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                                "Invalid partition flag '%s'", fv->data.string);
                    continue;
                }
                part->flags = g_list_prepend(part->flags, GINT_TO_POINTER(flag));
            }
            part->flags = g_list_reverse(part->flags);
        }

        GList *input_list = pu_hash_table_lookup_list(v->data.mapping, "input", NULL);
        if (input_list != NULL) {
            for (GList *i = input_list; i != NULL; i = i->next) {
                PuConfigValue *iv = i->data;
                if (iv->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
                    g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                                "'input' does not contain sequence of mappings");
                    return FALSE;
                }

                PuEmmcInput *input = g_new0(PuEmmcInput, 1);
                input->uri = pu_hash_table_lookup_string(iv->data.mapping, "uri", "");
                input->md5sum = pu_hash_table_lookup_string(iv->data.mapping, "md5sum", "");
                input->sha256sum = pu_hash_table_lookup_string(iv->data.mapping, "sha256sum", "");
                part->input = g_list_prepend(part->input, input);

                g_debug("Parsed partition input: uri=%s md5sum=%s sha256sum=%s",
                        input->uri, input->md5sum, input->sha256sum);
            }
            part->input = g_list_reverse(part->input);
        }

        if (part->size == 0 && !part->expand) {
            g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                        "Partition with invalid fixed size zero specified");
            return FALSE;
        }

        emmc->partitions = g_list_prepend(emmc->partitions, part);

        fixed_parts_size += part->size + part->offset;
        if (part->expand) {
            emmc->num_expanded_parts++;
        }
        if (part->type == PED_PARTITION_LOGICAL) {
            emmc->num_logical_parts++;
        }
    }

    emmc->partitions = g_list_reverse(emmc->partitions);

    if (emmc->num_expanded_parts > 0) {
        emmc->expanded_part_size = emmc->device->length - fixed_parts_size
                                   - 2 * emmc->num_logical_parts;

        /* Reserve blocks for secondary GPT */
        if (g_str_equal(emmc->disktype->name, "gpt")) {
            emmc->expanded_part_size -= PARTITION_TABLE_SIZE_GPT;
        }

        emmc->expanded_part_size /= emmc->num_expanded_parts;
        g_debug("%u expanding partitions, each of size %llds",
                emmc->num_expanded_parts, emmc->expanded_part_size);
    }

    return TRUE;
}

PuEmmc *
pu_emmc_new(const gchar *device_path,
            PuConfig *config,
            const gchar *prefix,
            gboolean skip_checksums,
            GError **error)
{
    PuEmmc *self;
    GHashTable *root;

    g_return_val_if_fail(device_path != NULL, NULL);
    g_return_val_if_fail(!g_str_equal(device_path, ""), NULL);
    g_return_val_if_fail(config != NULL, NULL);
    g_return_val_if_fail(PU_IS_CONFIG(config), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    self = g_object_new(PU_TYPE_EMMC,
                        "device-path", device_path,
                        "config", config,
                        "prefix", prefix,
                        "skip-checksums", skip_checksums,
                        NULL);
    root = pu_config_get_root(config);

    self->device = ped_device_get(device_path);
    if (!self->device) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Failed getting device '%s'", device_path);
        return NULL;
    }

    ped_unit_set_default(PED_UNIT_SECTOR);

    g_autofree gchar *disklabel = pu_hash_table_lookup_string(root, "disklabel", "msdos");
    self->disktype = ped_disk_type_get(disklabel);
    if (!self->disktype) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Disklabel '%s' is not supported by libparted", disklabel);
        return NULL;
    }

    if (!pu_emmc_parse_emmc_bootpart(self, root, error))
        return NULL;
    if (!pu_emmc_parse_raw(self, root, error))
        return NULL;
    if (!pu_emmc_parse_partitions(self, root, error))
        return NULL;
    if (!pu_emmc_parse_clean(self, root, error))
        return NULL;

    return g_steal_pointer(&self);
}
