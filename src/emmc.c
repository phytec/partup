/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <parted/parted.h>
#include <glib/gstdio.h>
#include "mount.h"
#include "utils.h"
#include "emmc.h"
#include "error.h"

struct _PuEmmcPartition {
    gchar *label;
    PedPartitionType type;
    gchar *filesystem;
    PedSector size;
    PedSector offset;
    gboolean expand;
    gchar *input;
};
struct _PuEmmcBinary {
    PedSector input_offset;
    PedSector output_offset;
    gchar *input;
};
struct _PuEmmcBootPartitions {
    gboolean enable;
    PedSector input_offset;
    PedSector output_offset;
    gchar *input;
};

typedef struct _PuEmmcPartition PuEmmcPartition;
typedef struct _PuEmmcBinary PuEmmcBinary;
typedef struct _PuEmmcBootPartitions PuEmmcBootPartitions;

struct _PuEmmc {
    PuFlash parent_instance;

    PedSector expanded_part_size;
    guint num_expanded_parts;

    PedDevice *device;
    PedDisk *disk;

    PedDiskType *disktype;
    GList *partitions;
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
    PedFileSystemType *fstype;
    PedConstraint *constraint;
    PedSector length = partition->size - 1;

    fstype = ped_file_system_type_get(partition->filesystem);
    if (fstype == NULL) {
        g_set_error(error, PU_ERROR, PU_ERROR_FLASH_LAYOUT,
                    "Failed to recognize filesystem type '%s'", partition->filesystem);
        return FALSE;
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
        g_debug("logical partition: start=%lld, length=%lld", start, length);
        start += 1;
        length -= 1;
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

        g_debug("type=%d filesystem=%s, start=%lld, size=%lld, offset=%lld, expand=%s",
                part->type, part->filesystem, part_start, part->size, part->offset,
                part->expand ? "y" : "n");

        if (!emmc_create_partition(self, part, part_start + part->offset, error)) {
            return FALSE;
        }
        part_start += part->size + part->offset;
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
    guint i = 0;
    gboolean first_logical_part = FALSE;
    g_autofree gchar *part_path = NULL;
    g_autofree gchar *part_mount = NULL;

    g_debug(G_STRFUNC);

    g_return_val_if_fail(flash != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    for (GList *p = self->partitions; p != NULL; p = p->next) {
        PuEmmcPartition *part = p->data;
        if (part->type == PED_PARTITION_LOGICAL && first_logical_part == FALSE) {
            first_logical_part = TRUE;
            i = 5;
        } else {
            i++;
        }
        part_path = g_strdup_printf("%sp%u", self->device->path, i);

        g_debug("Creating filesystem %s on %s", part->filesystem, part_path);
        g_return_val_if_fail(pu_make_filesystem(part_path, part->filesystem), FALSE);

        if (g_str_equal(part->input, "")) {
            g_debug("No input specified. Skipping %s",
                    g_strdup_printf("%sp%u", self->device->path, i));
            continue;
        }

        part_mount = pu_create_mount_point(g_strdup_printf("p%u", i));
        if (part_mount == NULL) {
            g_warning("Failed creating mount point");
            continue;
        }

        g_debug("Writing input data '%s' to %s (mounted at %s)",
                part->input, part_path, part_mount);

        pu_mount(part_path, part_mount);
        gchar **input_array = g_strsplit(part->input, " ", -1);
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
        g_rmdir(part_mount);
    }

    for (GList *b = self->raw; b != NULL; b = b->next) {
        PuEmmcBinary *bin = b->data;
        if (g_str_equal(bin->input, "")) {
            g_set_error(error, PU_ERROR, PU_ERROR_FLASH_DATA,
                        "No input specified for binary");
            continue;
        }

        pu_write_raw(bin->input, self->device->path, self->device->sector_size,
                     bin->input_offset, bin->output_offset);
    }

    pu_write_raw_bootpart(self->emmc_boot_partitions->input, self->device, 0,
                          self->emmc_boot_partitions->input_offset,
                          self->emmc_boot_partitions->output_offset);
    pu_write_raw_bootpart(self->emmc_boot_partitions->input, self->device, 1,
                          self->emmc_boot_partitions->input_offset,
                          self->emmc_boot_partitions->output_offset);
    pu_bootpart_enable(self->device->path, self->emmc_boot_partitions->enable);

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
        g_free(part->filesystem);
        g_free(part->input);
    }
    g_list_free(g_steal_pointer(&emmc->partitions));

    for (GList *b = emmc->raw; b != NULL; b = b->next) {
        PuEmmcBinary *bin = b->data;
        g_free(bin->input);
    }
    g_list_free(g_steal_pointer(&emmc->raw));

    g_free(emmc->emmc_boot_partitions->input);

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
            PuConfig *config)
{
    g_return_val_if_fail(device_path != NULL, NULL);
    g_return_val_if_fail(!g_str_equal(device_path, ""), NULL);
    g_return_val_if_fail(config != NULL, NULL);
    g_return_val_if_fail(PU_IS_CONFIG(config), NULL);

    PuEmmc *self = g_object_new(PU_TYPE_EMMC,
                                "device-path", device_path,
                                "config", config,
                                NULL);
    GHashTable *root = pu_config_get_root(config);

    self->device = ped_device_get(device_path);
    g_return_val_if_fail(self->device != NULL, NULL);
    self->num_expanded_parts = 0;

    ped_unit_set_default(PED_UNIT_SECTOR);

    g_autofree gchar *disklabel = pu_hash_table_lookup_string(root, "disklabel", "msdos");
    self->disktype = ped_disk_type_get(disklabel);
    g_return_val_if_fail(self->disktype != NULL, NULL);

    PedSector fixed_parts_size = 0;
    GList *partitions = g_hash_table_lookup(root, "partitions");
    g_return_val_if_fail(partitions != NULL, NULL);

    for (GList *p = partitions; p != NULL; p = p->next) {
        PuEmmcPartition *part = g_new0(PuEmmcPartition, 1);
        part->label = pu_hash_table_lookup_string(p->data, "label", "");
        part->filesystem = pu_hash_table_lookup_string(p->data, "filesystem", "");
        part->size = pu_hash_table_lookup_sector(p->data, self->device, "size", 0);
        part->offset = pu_hash_table_lookup_sector(p->data, self->device, "offset", 0);
        part->expand = pu_hash_table_lookup_boolean(p->data, "expand", FALSE);
        part->input = pu_hash_table_lookup_string(p->data, "input", "");

        g_autofree gchar *type_str = pu_hash_table_lookup_string(p->data, "type", "primary");
        if (g_str_equal(type_str, "primary"))
            part->type = PED_PARTITION_NORMAL;
        else if (g_str_equal(type_str, "logical"))
            part->type = PED_PARTITION_LOGICAL;
        else
            g_critical("Partition of invalid type '%s' specified", type_str);

        if (part->size == 0 && !part->expand) {
            g_critical("Partition with invalid fixed size zero specified");
        }

        self->partitions = g_list_prepend(self->partitions, part);

        fixed_parts_size += part->size + part->offset;
        if (part->expand) {
            self->num_expanded_parts++;
        }
    }

    self->partitions = g_list_reverse(self->partitions);

    if (self->num_expanded_parts > 0) {
        self->expanded_part_size = (self->device->length - fixed_parts_size)
                                    / self->num_expanded_parts;
        g_debug("%u expanding partitions, each of size %llds",
                self->num_expanded_parts, self->expanded_part_size);
    }

    GList *raw = g_hash_table_lookup(root, "raw");
    g_return_val_if_fail(raw != NULL, NULL);

    for (GList *b = raw; b != NULL; b = b->next) {
        PuEmmcBinary *bin = g_new0(PuEmmcBinary, 1);
        bin->input_offset = pu_hash_table_lookup_sector(b->data, self->device, "input-offset", 0);
        bin->output_offset = pu_hash_table_lookup_sector(b->data, self->device, "output-offset", 0);
        bin->input = pu_hash_table_lookup_string(b->data, "input", "");

        self->raw = g_list_prepend(self->raw, bin);
    }

    self->raw = g_list_reverse(self->raw);

    GHashTable *bootpart = g_hash_table_lookup(root, "emmc-boot-partitions");
    g_return_val_if_fail(bootpart != NULL, NULL);
    self->emmc_boot_partitions = g_new0(PuEmmcBootPartitions, 1);
    self->emmc_boot_partitions->enable =
        pu_hash_table_lookup_boolean(bootpart, "enable", FALSE);
    self->emmc_boot_partitions->input_offset =
        pu_hash_table_lookup_sector(bootpart, self->device, "input-offset", 0);
    self->emmc_boot_partitions->output_offset =
        pu_hash_table_lookup_sector(bootpart, self->device, "output-offset", 0);
    self->emmc_boot_partitions->input =
        pu_hash_table_lookup_string(bootpart, "input", "");

    return g_steal_pointer(&self);
}
