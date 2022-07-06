/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <parted/parted.h>
#include <glib/gstdio.h>
#include "mount.h"
#include "utils.h"
#include "configemmc.h"
#include "emmc.h"
#include "error.h"

struct _PuEmmc {
    PuFlash parent_instance;

    gint64 expanded_part_size;
    guint num_expanded_parts;

    PedDevice *device;
    PedDisk *disk;
};

G_DEFINE_TYPE(PuEmmc, pu_emmc, PU_TYPE_FLASH)

#define sectors_to_bytes(sectors) \
    (sectors * self->device->sector_size)
#define sectors_to_kibibytes(sectors) \
    (sectors * self->device->sector_size / PED_KIBIBYTE_SIZE)
#define sectors_to_kilobytes(sectors) \
    (sectors * self->device->sector_size / PED_KILOBYTE_SIZE)
#define sectors_to_mebibytes(sectors) \
    (sectors * self->device->sector_size / PED_MEBIBYTE_SIZE)
#define sectors_to_megabytes(sectors) \
    (sectors * self->device->sector_size / PED_MEGABYTE_SIZE)
#define sectors_to_gibibytes(sectors) \
    (sectors * self->device->sector_size / PED_GIBIBYTE_SIZE)
#define sectors_to_gigabytes(sectors) \
    (sectors * self->device->sector_size / PED_GIGABYTE_SIZE)

static gboolean
emmc_create_partition(PuEmmc *self,
                      const gchar *label,
                      PedPartitionType type,
                      const gchar *filesystem,
                      gint64 start,
                      gint64 length,
                      GError **error)
{
    PedPartition *part;
    PedFileSystemType *fstype;
    PedConstraint *constraint;

    fstype = ped_file_system_type_get(filesystem);
    if (fstype == NULL) {
        g_set_error(error, PU_ERROR, PU_ERROR_FLASH_LAYOUT,
                    "Failed to recognize filesystem type '%s'", filesystem);
        return FALSE;
    }

    if (type == PED_PARTITION_LOGICAL) {
        if (!ped_disk_type_check_feature(self->disk->type, PED_DISK_TYPE_EXTENDED)) {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_LAYOUT,
                        "Logical partitions are not supported on this disk");
            return FALSE;
        }
        if (ped_disk_extended_partition(self->disk) == NULL) {
            g_debug("Creating extended partition at %ld", start);
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
        g_debug("logical partition: start=%ld, length=%ld", start, length);
        start += 1;
        length -= 1;
    }

    part = ped_partition_new(self->disk, type, fstype, start, start + length);
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
        ped_partition_set_name(part, label);
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
    PuConfig *config;
    PedDisk *newdisk;
    PedDiskType *disktype;
    gchar *disklabel;

    g_debug(G_STRFUNC);

    g_return_val_if_fail(flash != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_object_get(flash, "config", &config, NULL);

    g_return_val_if_fail(config != NULL, FALSE);
    g_return_val_if_fail(PU_IS_CONFIG_EMMC(config), FALSE);

    disklabel = pu_config_emmc_get_disklabel(PU_CONFIG_EMMC(config));
    disktype = ped_disk_type_get(disklabel);
    newdisk = ped_disk_new_fresh(self->device, disktype);
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
    PuConfig *config;
    gint64 part_start = 0;
    PedPartitionType type;
    g_autofree gchar *part_label = NULL;
    g_autofree gchar *part_type = NULL;
    g_autofree gchar *part_fs = NULL;
    gint64 part_offset;
    gint64 part_size;

    g_debug(G_STRFUNC);

    g_return_val_if_fail(flash != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_object_get(flash, "config", &config, NULL);

    g_return_val_if_fail(config != NULL, FALSE);
    g_return_val_if_fail(PU_IS_CONFIG_EMMC(config), FALSE);

    GList *partitions = pu_config_emmc_get_partitions(PU_CONFIG_EMMC(config));

    for (GList *part = partitions; part != NULL; part = part->next) {
        part_label = pu_hash_table_lookup_string(part->data, "label", "");
        part_type = pu_hash_table_lookup_string(part->data, "type", "primary");
        part_fs = pu_hash_table_lookup_string(part->data, "filesystem", "fat32");
        part_offset = pu_hash_table_lookup_sector(part->data, self->device, "offset", 0);
        part_size = pu_hash_table_lookup_sector(part->data, self->device, "size", 2048);

        if (g_str_equal(part_type, "primary")) {
            type = PED_PARTITION_NORMAL;
        } else if (g_str_equal(part_type, "logical")) {
            type = PED_PARTITION_LOGICAL;
        } else {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_LAYOUT,
                        "Partition of unkown type '%s' specified", part_type);
            continue;
        }

        if (part_size == 0) {
            part_size = self->expanded_part_size;
        }

        g_debug("type=%s filesystem=%s, start=%ld, size=%ld, offset=%ld",
                part_type, part_fs, part_start, part_size, part_offset);

        if (!emmc_create_partition(self, part_label, type, part_fs,
                                   part_start + part_offset, part_size - 1, error)) {
            return FALSE;
        }
        part_start += part_size + part_offset;
    }

    ped_disk_commit(self->disk);

    g_debug("%s: Finished", G_STRFUNC);

    return TRUE;
}

static gboolean
pu_emmc_write_data(PuFlash *flash,
                   GError **error)
{
    PuEmmc *self = PU_EMMC(flash);
    PuConfig *config;
    g_autofree gchar *device_path = NULL;
    g_autofree gchar *part_label = NULL;
    g_autofree gchar *part_input = NULL;
    g_autofree gchar *part_type = NULL;
    g_autofree gchar *part_fs = NULL;
    guint i = 0;
    gboolean first_logical_part = FALSE;
    g_autofree gchar *part_path = NULL;
    g_autofree gchar *part_mount = NULL;

    g_debug(G_STRFUNC);

    g_return_val_if_fail(flash != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_object_get(flash, "config", &config, "device-path", &device_path, NULL);
    g_return_val_if_fail(config != NULL, FALSE);
    g_return_val_if_fail(PU_IS_CONFIG_EMMC(config), FALSE);

    GList *partitions = pu_config_emmc_get_partitions(PU_CONFIG_EMMC(config));
    GList *raw = pu_config_emmc_get_raw(PU_CONFIG_EMMC(config));
    GHashTable *emmc_bootpart = pu_config_emmc_get_bootpart(PU_CONFIG_EMMC(config));

    for (GList *part = partitions; part != NULL; part = part->next) {
        part_label = pu_hash_table_lookup_string(part->data, "label", "default_mount");
        part_input = pu_hash_table_lookup_string(part->data, "input", "");
        part_type = pu_hash_table_lookup_string(part->data, "type", "primary");
        part_fs = pu_hash_table_lookup_string(part->data, "filesystem", "fat32");

        if (g_str_equal(part_type, "logical") && first_logical_part == FALSE) {
            first_logical_part = TRUE;
            i = 5;
        } else {
            i++;
        }
        part_path = g_strdup_printf("%sp%u", device_path, i);

        g_debug("Creating filesystem %s on %s", part_fs, part_path);
        g_return_val_if_fail(pu_make_filesystem(part_path, part_fs), 1);

        if (g_str_equal(part_input, "")) {
            g_debug("No input specified. Skipping %s",
                    g_strdup_printf("%sp%u", device_path, i));
            continue;
        }

        part_mount = pu_create_mount_point(part_label);
        if (part_mount == NULL) {
            g_warning("Failed creating mount point");
            continue;
        }

        g_debug("Writing input data '%s' to %s (mounted at %s)",
                part_input, part_path, part_mount);

        pu_mount(part_path, part_mount);
        gchar **input_array = g_strsplit(part_input, " ", -1);
        for (gchar **i = input_array; *i != NULL; i++) {
            if (g_regex_match_simple(".tar", *i, G_REGEX_CASELESS, 0)) {
                g_debug("Extracting '%s' to '%s'", *i, part_mount);
                pu_archive_extract(*i, part_mount);
            } else {
                g_debug("Copying '%s' to '%s'", *i, part_mount);
                pu_copy_file(*i, part_mount);
            }
        }
        g_strfreev(input_array);
        pu_umount(part_mount);
    }
    /* FIXME: This silently fails for some reason. The directory is not removed. */
    if (!g_rmdir(PU_MOUNT_PREFIX)) {
        g_set_error(error, PU_ERROR, PU_ERROR_FLASH_DATA,
                    "Failed cleaning up mount point %s", PU_MOUNT_PREFIX);
    }

    for (GList *bin = raw; bin != NULL; bin = bin->next) {
        PedSector rio = pu_hash_table_lookup_sector(bin->data, self->device, "input-offset", 0);
        PedSector roo = pu_hash_table_lookup_sector(bin->data, self->device, "output-offset", 0);
        gchar *input = pu_hash_table_lookup_string(bin->data, "input", "");

        if (g_str_equal(input, "")) {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_DATA,
                        "No input specified for binary");
            continue;
        }

        pu_write_raw(input, device_path, self->device->sector_size, rio, roo);
    }

    gboolean bootpart_enable = pu_hash_table_lookup_boolean(emmc_bootpart, "enabled", FALSE);
    PedSector bootpart_io = pu_hash_table_lookup_sector(emmc_bootpart, self->device, "input-offset", 0);
    PedSector bootpart_oo = pu_hash_table_lookup_sector(emmc_bootpart, self->device, "output-offset", 0);
    gchar *bootpart_input = pu_hash_table_lookup_string(emmc_bootpart, "input", "");
    pu_write_raw_bootpart(bootpart_input, self->device, 0, bootpart_io, bootpart_oo);
    pu_write_raw_bootpart(bootpart_input, self->device, 1, bootpart_io, bootpart_oo);
    pu_bootpart_enable(device_path, bootpart_enable);

    g_debug("%s: Finished", G_STRFUNC);

    return TRUE;
}

static void
pu_emmc_class_finalize(GObject *object)
{
    PuEmmc *emmc = PU_EMMC(object);

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

PuEmmc *
pu_emmc_new(const gchar *device_path,
            PuConfigEmmc *config)
{
    gint64 part_size;
    gint64 part_offset;
    gint64 fixed_parts_size = 0;

    g_return_val_if_fail(device_path != NULL, NULL);
    g_return_val_if_fail(!g_str_equal(device_path, ""), NULL);
    g_return_val_if_fail(config != NULL, NULL);
    g_return_val_if_fail(PU_IS_CONFIG_EMMC(config), NULL);

    PuEmmc *self = g_object_new(PU_TYPE_EMMC,
                                "device-path", device_path,
                                "config", PU_CONFIG(config),
                                NULL);
    self->device = ped_device_get(device_path);
    g_return_val_if_fail(self->device != NULL, NULL);
    self->num_expanded_parts = 0;

    GList *partitions = pu_config_emmc_get_partitions(config);

    for (GList *part = partitions; part != NULL; part = part->next) {
        part_size = pu_hash_table_lookup_int64(part->data, "size", 0);
        part_offset = pu_hash_table_lookup_int64(part->data, "offset", 0);
        g_debug("partition %p: size=%ld offset=%ld", (gpointer) part, part_size, part_offset);
        fixed_parts_size += (part_size + part_offset) * PED_MEBIBYTE_SIZE;
        if (part_size == 0) {
            self->num_expanded_parts++;
        }
    }

    self->expanded_part_size = (self->device->length * self->device->sector_size
                                - fixed_parts_size) / (self->num_expanded_parts * PED_MEBIBYTE_SIZE);
    g_debug("%u expanding partitions, each of size %ld",
            self->num_expanded_parts, self->expanded_part_size);

    return g_steal_pointer(&self);
}
