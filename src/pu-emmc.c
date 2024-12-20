/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#define G_LOG_DOMAIN "partup-emmc"

#include <parted/parted.h>
#include <glib/gstdio.h>
#include "pu-checksum.h"
#include "pu-error.h"
#include "pu-file.h"
#include "pu-hashtable.h"
#include "pu-mount.h"
#include "pu-utils.h"
#include "pu-emmc.h"

#define PARTITION_TABLE_SIZE_MSDOS 1
#define PARTITION_TABLE_SIZE_GPT   34

typedef struct _PuEmmcInput {
    gchar *filename;
    gchar *md5sum;
    gchar *sha256sum;

    /* Internal members */
    gsize _size;
} PuEmmcInput;
typedef struct _PuEmmcPartition {
    gchar *label;
    gchar *partuuid;
    PedPartitionType type;
    gchar *filesystem;
    gchar *mkfs_extra_args;
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
    guint enable;
    gboolean boot_ack;
    GList *input;
} PuEmmcBootPartitions;
typedef struct _PuEmmcControls {
    gchar *hwreset;
    gchar *bootbus;
    PuEmmcBootPartitions *boot_partitions;
} PuEmmcControls;
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
    PuEmmcControls *mmc_controls;
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

    /* Set PARTLABEL on GPT disks */
    if (ped_disk_type_check_feature(part->disk->type, PED_DISK_TYPE_PARTITION_NAME) &&
        partition->label) {
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

    g_return_val_if_fail(flash != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (self->disktype == NULL) {
        g_debug("Nothing to initialize");
        return TRUE;
    }

    g_message("Initializing MMC with new disklabel");

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

    return TRUE;
}

static gboolean
pu_emmc_setup_layout(PuFlash *flash,
                     GError **error)
{
    PuEmmc *self = PU_EMMC(flash);
    PedSector part_start = 0;

    g_return_val_if_fail(flash != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (self->disktype == NULL) {
        g_debug("No partitions to set up");
        return TRUE;
    }

    g_message("Partitioning MMC");

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

        g_debug("Creating partition: type=%d filesystem=%s start=%lld size=%lld "
                "offset=%lld block-size=%lld expand=%s",
                part->type, part->filesystem, part_start, part->size,
                part->offset, part->block_size, part->expand ? "true" : "false");

        if (!emmc_create_partition(self, part, part_start + part->offset, error)) {
            return FALSE;
        }
        part_start += part->size + part->offset;
    }

    ped_disk_commit(self->disk);

    if (!pu_wait_for_partitions(error))
        return FALSE;

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

    g_return_val_if_fail(flash != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_object_get(flash,
                 "prefix", &prefix,
                 "skip-checksums", &skip_checksums,
                 NULL);

    g_message("Writing data to MMC");

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

        g_debug("Creating filesystem '%s' on '%s'", part->filesystem, part_path);

        if (!pu_make_filesystem(part_path, part->filesystem, part->label,
                                part->mkfs_extra_args, error))
            return FALSE;

        if (!part->input) {
            g_debug("No input specified. Skipping '%s'", part_path);
            continue;
        }

        g_debug("Writing to partition '%s'", part_path);

        part_mount = pu_create_mount_point(g_strdup_printf("p%u", i), error);
        if (part_mount == NULL)
            return FALSE;

        for (GList *i = part->input; i != NULL; i = i->next) {
            PuEmmcInput *input = i->data;
            g_autofree gchar *path = NULL;

            path = pu_path_from_filename(input->filename, prefix, error);
            if (path == NULL) {
                g_prefix_error(error, "Failed parsing input filename for partition: ");
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
                if (!pu_mount(part_path, part_mount, NULL, NULL, error))
                    return FALSE;
                if (!pu_archive_extract(path, part_mount, error))
                    return FALSE;
                if (!pu_umount(part_mount, error))
                    return FALSE;
            } else if (g_regex_match_simple(".ext[234]$", path, 0, 0)) {
                if (!pu_write_raw(path, part_path, self->device, 0, 0, 0, error))
                    return FALSE;
                if (!pu_resize_filesystem(part_path, error))
                    return FALSE;
                if (!pu_set_ext_label(part_path, part->label, error))
                    return FALSE;
            } else {
                if (!pu_mount(part_path, part_mount, NULL, NULL, error))
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

        g_debug("Cleaning at offset %lld with size %lld",
                clean->offset, clean->size);

        if (!pu_write_raw("/dev/zero", self->device->path, self->device, 0,
                          clean->offset, clean->size, error)) {
            return FALSE;
        }
    }

    for (GList *b = self->raw; b != NULL; b = b->next) {
        PuEmmcBinary *bin = b->data;
        PuEmmcInput *input = bin->input;
        g_autofree gchar *path = NULL;
        gsize size = 0;
        g_autofree gchar *output_sha256sum = NULL;

        path = pu_path_from_filename(input->filename, prefix, error);
        if (path == NULL) {
            g_prefix_error(error, "Failed parsing input filename for binary: ");
            return FALSE;
        }

        if (g_str_equal(path, "")) {
            g_warning("No input specified for binary");
            continue;
        }

        size = pu_file_get_size(path, error);
        if (size == 0) {
            g_prefix_error(error, "Failed retrieving file size for binary: ");
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
        output_sha256sum = pu_checksum_new_from_file(path, bin->input_offset *
                                                     self->device->sector_size,
                                                     G_CHECKSUM_SHA256, error);

        g_debug("Writing raw data: filename=%s input_offset=%lld output_offset=%lld",
                input->filename, bin->input_offset, bin->output_offset);

        if (!pu_write_raw(path, self->device->path, self->device,
                          bin->input_offset, bin->output_offset, 0, error))
            return FALSE;

        if (!skip_checksums) {
            g_debug("Checking SHA256 sum of written output: %s", output_sha256sum);
            if (!pu_checksum_verify_raw(self->device->path, bin->output_offset *
                                        self->device->sector_size,
                                        size - bin->input_offset *
                                        self->device->sector_size, output_sha256sum,
                                        G_CHECKSUM_SHA256, error))
                return FALSE;
        }
    }

    if (self->mmc_controls) {
        PuEmmcBootPartitions *boot_partitions = NULL;

        if (pu_has_bootpart(self->device->path)) {
            if (!pu_set_hwreset(self->device->path,
                                   self->mmc_controls->hwreset, error))
                return FALSE;

            if (!pu_set_bootbus(self->device->path,
                                self->mmc_controls->bootbus, error))
                return FALSE;
        }

        boot_partitions = self->mmc_controls->boot_partitions;
        if (boot_partitions && pu_has_bootpart(self->device->path)) {
            GList *input = boot_partitions->input;

            if (!pu_bootpart_enable(self->device->path, boot_partitions->enable,
                                    boot_partitions->boot_ack, error))
                return FALSE;

            for (GList *i = input; i != NULL; i = i->next) {
                PuEmmcBinary *bin = i->data;
                g_autofree gchar *path = NULL;

                path = pu_path_from_filename(bin->input->filename, prefix, error);
                if (path == NULL) {
                    g_prefix_error(error, "Failed parsing input filename for eMMC boot partition: ");
                    return FALSE;
                }

                if (g_str_equal(path, "")) {
                    g_set_error(error, PU_ERROR, PU_ERROR_FLASH_DATA,
                                "No input specified for eMMC boot partition");
                    return FALSE;
                }

                if (!g_str_equal(bin->input->md5sum, "") && !skip_checksums) {
                    g_debug("Checking MD5 sum of input file '%s'", path);
                    if (!pu_checksum_verify_file(path, bin->input->md5sum,
                                                 G_CHECKSUM_MD5, error))
                        return FALSE;
                }
                if (!g_str_equal(bin->input->sha256sum, "") && !skip_checksums) {
                    g_debug("Checking SHA256 sum of input file '%s'", path);
                    if (!pu_checksum_verify_file(path, bin->input->sha256sum,
                                                 G_CHECKSUM_SHA256, error))
                        return FALSE;
                }

                g_debug("Writing eMMC boot partitions: filename=%s input_offset=%lld output_offset=%lld",
                        bin->input->filename, bin->input_offset,
                        bin->output_offset);

                if (!pu_write_raw_bootpart(path, self->device, 0,
                                           bin->input_offset,
                                           bin->output_offset,
                                           error))
                    return FALSE;

                if (!pu_write_raw_bootpart(path, self->device, 1,
                                           bin->input_offset,
                                           bin->output_offset,
                                           error))
                    return FALSE;
            }
        }
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
        g_free(part->mkfs_extra_args);
        g_list_free(g_steal_pointer(&part->flags));
        for (GList *i = part->input; i != NULL; i = i->next) {
            PuEmmcInput *in = i->data;
            g_free(in->filename);
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
        g_free(bin->input->filename);
        g_free(bin->input->md5sum);
        g_free(bin->input->sha256sum);
        g_free(bin->input);
        g_free(bin);
    }
    g_list_free(g_steal_pointer(&emmc->raw));

    if (emmc->mmc_controls) {
        g_free(emmc->mmc_controls->hwreset);
        g_free(emmc->mmc_controls->bootbus);
        if (emmc->mmc_controls->boot_partitions) {
            GList *input = emmc->mmc_controls->boot_partitions->input;
            for (GList *b = input; b != NULL; b = b->next) {
                PuEmmcBinary *bin = b->data;
                g_free(bin->input->filename);
                g_free(bin->input->md5sum);
                g_free(bin->input->sha256sum);
                g_free(bin->input);
                g_free(bin);
            }
            g_list_free(g_steal_pointer(&input));
            g_free(emmc->mmc_controls->boot_partitions);
        }
        g_free(emmc->mmc_controls);
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

static PedSector
pu_emmc_get_partition_table_size(PuEmmc *emmc)
{
    if (emmc->disktype == NULL)
        return 0;
    else if (g_str_equal(emmc->disktype->name, "msdos"))
        return PARTITION_TABLE_SIZE_MSDOS;
    else if (g_str_equal(emmc->disktype->name, "gpt"))
        return PARTITION_TABLE_SIZE_GPT;

    return 1;
}

static gboolean
pu_emmc_parse_mmc_controls(PuEmmc *emmc,
                           GHashTable *root,
                           GError **error)
{
    PuConfigValue *value_mmc;
    PuConfigValue *value_bootpart;

    g_return_val_if_fail(emmc != NULL, FALSE);
    g_return_val_if_fail(root != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    value_mmc = g_hash_table_lookup(root, "mmc");
    if (!value_mmc) {
        g_debug("No entry 'mmc' found. Skipping...");
        return TRUE;
    }
    if (value_mmc->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
        g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                    "'mmc' is not a valid YAML mapping");
        return FALSE;
    }

    GHashTable *mmc_control = value_mmc->data.mapping;
    emmc->mmc_controls = g_new0(PuEmmcControls, 1);
    emmc->mmc_controls->hwreset=
        pu_hash_table_lookup_string(mmc_control, "hwreset", NULL);
    emmc->mmc_controls->bootbus =
        pu_hash_table_lookup_string(mmc_control, "bootbus", NULL);

    value_bootpart = g_hash_table_lookup(mmc_control, "boot-partitions");
    if (!value_bootpart) {
        g_debug("No entry 'boot-partitions' found. Skipping...");
        return TRUE;
    }
    if (value_bootpart->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
        g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                    "'boot-partitions' is not a valid YAML mapping");
        return FALSE;
    }

    GHashTable *bootpart = value_bootpart->data.mapping;
    emmc->mmc_controls->boot_partitions = g_new0(PuEmmcBootPartitions, 1);
    emmc->mmc_controls->boot_partitions->enable =
        pu_hash_table_lookup_int64(bootpart, "enable", 0);
    emmc->mmc_controls->boot_partitions->boot_ack =
        pu_hash_table_lookup_boolean(bootpart, "boot-ack", FALSE);

    g_debug("Parsed bootpart: enable=%d boot-ack=%s",
            emmc->mmc_controls->boot_partitions->enable,
            emmc->mmc_controls->boot_partitions->boot_ack ? "true" : "false");

    GList *input_list = pu_hash_table_lookup_list(bootpart, "binaries", NULL);
    for (GList *b = input_list; b != NULL; b = b->next) {
        PuConfigValue *v = b->data;
        PuEmmcBinary *bin = g_new0(PuEmmcBinary, 1);
        bin->input_offset = pu_hash_table_lookup_sector(v->data.mapping, emmc->device,
                                                        "input-offset", 0);
        bin->output_offset = pu_hash_table_lookup_sector(v->data.mapping, emmc->device,
                                                         "output-offset", 0);

        PuConfigValue *value_input = g_hash_table_lookup(v->data.mapping, "input");
        if (value_input == NULL) {
            g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                        "No input specified for boot partition binaries");
            return FALSE;
        }
        if (value_input->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
            g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                        "'input' of binary does not contain a mapping");
            return FALSE;
        }

        PuEmmcInput *input = g_new0(PuEmmcInput, 1);
        input->filename = pu_hash_table_lookup_string(value_input->data.mapping, "filename", "");
        input->md5sum = pu_hash_table_lookup_string(value_input->data.mapping, "md5sum", "");
        input->sha256sum = pu_hash_table_lookup_string(value_input->data.mapping, "sha256sum", "");

        bin->input = input;
        g_debug("Parsed bootpart input: filename=%s md5sum=%s sha256sum=%s",
                input->filename, input->md5sum, input->sha256sum);

        emmc->mmc_controls->boot_partitions->input =
            g_list_prepend(emmc->mmc_controls->boot_partitions->input, bin);
    }

    emmc->mmc_controls->boot_partitions->input
        = g_list_reverse(emmc->mmc_controls->boot_partitions->input);

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
    g_autofree gchar *path = NULL;
    g_autofree gchar *prefix = NULL;

    g_return_val_if_fail(emmc != NULL, FALSE);
    g_return_val_if_fail(root != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_object_get(emmc,
                 "prefix", &prefix,
                 NULL);

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
        input->filename = pu_hash_table_lookup_string(value_input->data.mapping, "filename", "");
        input->md5sum = pu_hash_table_lookup_string(value_input->data.mapping, "md5sum", "");
        input->sha256sum = pu_hash_table_lookup_string(value_input->data.mapping, "sha256sum", "");

        path = pu_path_from_filename(input->filename, prefix, error);
        if (path == NULL)
            return FALSE;

        input->_size = pu_file_get_size(path, error);
        if (!input->_size)
            return FALSE;

        bin->input = input;
        g_debug("Parsed raw input: filename=%s md5sum=%s sha256sum=%s",
                input->filename, input->md5sum, input->sha256sum);

        PedSector min_offset = pu_emmc_get_partition_table_size(emmc);

        if (bin->output_offset < min_offset) {
            g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                        "Offset of raw binary is too small and would "
                        "overwrite partition table");
            return FALSE;
        }

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
        part->label = pu_hash_table_lookup_string(v->data.mapping, "label", NULL);
        part->partuuid = pu_hash_table_lookup_string(v->data.mapping, "partuuid", "");
        part->filesystem = pu_hash_table_lookup_string(v->data.mapping, "filesystem", "fat32");
        part->mkfs_extra_args = pu_hash_table_lookup_string(v->data.mapping, "mkfs-extra-args", NULL);
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
            PedSector min_offset = pu_emmc_get_partition_table_size(emmc);

            if (part->offset < min_offset && part->offset > 0) {
                g_set_error(error, PU_ERROR, PU_ERROR_EMMC_PARSE,
                            "Offset of first partition too small and would "
                            "overwrite partition table");
                return FALSE;
            }

            if (part->offset == 0) {
                g_warning("No offset specified for first partition (using "
                          "default of %lld sectors)", min_offset);
                part->offset = min_offset;
            }
        }

        g_debug("Parsed partition: label=%s filesystem=%s mkfs-extra-args=%s "
                "type=%s size=%lld offset=%lld block-size=%lld expand=%s",
                part->label, part->filesystem, part->mkfs_extra_args, type_str,
                part->size, part->offset, part->block_size,
                part->expand ? "true" : "false");

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
                input->filename = pu_hash_table_lookup_string(iv->data.mapping, "filename", "");
                input->md5sum = pu_hash_table_lookup_string(iv->data.mapping, "md5sum", "");
                input->sha256sum = pu_hash_table_lookup_string(iv->data.mapping, "sha256sum", "");
                part->input = g_list_prepend(part->input, input);

                g_debug("Parsed partition input: filename=%s md5sum=%s sha256sum=%s",
                        input->filename, input->md5sum, input->sha256sum);
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

static gboolean
pu_emmc_check_raw_overwrite(PuEmmc *emmc,
                            GError **error)
{
    g_return_val_if_fail(emmc != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (emmc->partitions && emmc->raw) {
        PuEmmcPartition *part = emmc->partitions->data;
        gsize part_start = part->offset * emmc->device->sector_size;
        gsize raw_start;
        gsize raw_end;

        for (GList *b = emmc->raw; b != NULL; b = b->next) {
            PuEmmcBinary *bin = b->data;

            raw_start = bin->output_offset * emmc->device->sector_size;
            raw_end = bin->input->_size;
            raw_end += raw_start;
            raw_end -= bin->input_offset * emmc->device->sector_size;

            if (raw_end > part_start) {
                g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                            "Raw binary overlaps with first partition");
                return FALSE;
            }

            /* Check against all binaries after the current one */
            for (GList *i = b->next; i != NULL; i = i->next) {
                PuEmmcBinary *bin2 = i->data;

                gsize raw2_start = bin2->output_offset * emmc->device->sector_size;
                gsize raw2_end;
                raw2_end = bin2->input->_size;
                raw2_end += raw2_start;
                raw2_end -= bin2->input_offset * emmc->device->sector_size;

                if (!(raw_end < raw2_start || raw_start > raw2_end)) {
                    g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                                "Raw binary overlaps with other raw binary");
                    return FALSE;
                }
            }
        }
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

    g_autofree gchar *disklabel = pu_hash_table_lookup_string(root, "disklabel", NULL);
    if (disklabel == NULL) {
        g_debug("No disklabel specified! Skipping partitioning and overwrite checks...");
    } else {
        self->disktype = ped_disk_type_get(disklabel);
        if (!self->disktype) {
            g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                        "Disklabel '%s' is not supported by libparted", disklabel);
            return NULL;
        }
    }

    if (!pu_emmc_parse_mmc_controls(self, root, error))
        return NULL;
    if (!pu_emmc_parse_raw(self, root, error))
        return NULL;
    if (disklabel && !pu_emmc_parse_partitions(self, root, error))
        return NULL;
    if (!pu_emmc_parse_clean(self, root, error))
        return NULL;

    if (disklabel && !pu_emmc_check_raw_overwrite(self, error))
        return NULL;

    return g_steal_pointer(&self);
}
