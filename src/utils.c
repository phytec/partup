/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <fcntl.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "config.h"
#include "utils.h"

gboolean
pu_copy_file(const gchar *filename,
             const gchar *dest)
{
    g_autoptr(GFile) in = g_file_new_for_path(filename);
    g_autofree gchar *out_path = g_strdup_printf("%s/%s", dest,
                                                 g_path_get_basename(filename));
    g_autoptr(GFile) out = g_file_new_for_path(out_path);

    if (!g_file_copy(in, out, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL)) {
        g_critical("Failed copying file '%s' to '%s'", filename, out_path);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_archive_extract(const gchar *filename,
                   const gchar *dest)
{
    g_autofree gchar *cmd = g_strdup_printf("tar -xf %s -C %s", filename, dest);

    if (!g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL)) {
        g_critical("Failed extracting archive '%s' to '%s'!", filename, dest);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_make_filesystem(const gchar *part,
                   const gchar *type)
{
    g_autoptr(GString) cmd = g_string_new(NULL);

    if (g_strcmp0(type, "fat32") == 0) {
        g_string_append(cmd, "mkfs.vfat ");
    } else if (g_strcmp0(type, "ext2") == 0) {
        g_string_append(cmd, "mkfs.ext2 ");
    } else if (g_strcmp0(type, "ext3") == 0) {
        g_string_append(cmd, "mkfs.ext3 ");
    } else if (g_strcmp0(type, "ext4") == 0) {
        g_string_append(cmd, "mkfs.ext4 ");
    } else {
        g_critical("Unknown filesystem type \"%s\" used for \"%s\"!", type, part);
        return FALSE;
    }
    g_string_append(cmd, part);

    if (!g_spawn_command_line_sync(cmd->str, NULL, NULL, NULL, NULL)) {
        g_critical("Failed making filesystem \"%s\" on \"%s\"!", type, part);
        return FALSE;
    }

    return TRUE;
}

/* TODO: Use more g_file_* functions. This also allows for getting back the
 * actually written data for verification of their checksum. */
gboolean
pu_write_raw(const gchar *input,
             const gchar *output,
             PedSector block_size,
             PedSector input_offset,
             PedSector output_offset)
{
    g_autofree gchar *cmd = g_strdup_printf("dd if=%s of=%s bs=%lld skip=%lld seek=%lld",
                                            input, output, block_size,
                                            input_offset, output_offset);

    g_debug("Executing '%s'", cmd);
    if (!g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL)) {
        g_critical("Failed writing raw input '%s' to '%s'!",
                   input, output);
        return FALSE;
    }

    return TRUE;
}

static gboolean
pu_bootpart_force_ro(const gchar *bootpart,
                     gboolean read_only)
{
    g_autofree gchar *path = g_strdup_printf("/sys/class/block/%s/force_ro",
                                             g_path_get_basename(bootpart));
    FILE *file = g_fopen(path, "w");

    if (file == NULL) {
        g_critical("Failed opening %s!", path);
        return FALSE;
    }

    if (!g_fprintf(file, "%u", read_only)) {
        g_critical("Failed writing %d to %s!", read_only, path);
        fclose(file);
        return FALSE;
    }

    fclose(file);
    return TRUE;
}

gboolean
pu_write_raw_bootpart(const gchar *input,
                      PedDevice *device,
                      guint bootpart,
                      PedSector input_offset,
                      PedSector output_offset)
{
    g_return_val_if_fail(bootpart <= 1, FALSE);

    g_autofree gchar *bootpart_device = g_strdup_printf("%sboot%d", device->path, bootpart);
    g_autofree gchar *cmd = g_strdup_printf("dd if=%s of=%s bs=%lld skip=%lld seek=%lld",
                                            input, bootpart_device, device->sector_size,
                                            input_offset, output_offset);

    g_debug("Executing '%s'", cmd);
    g_return_val_if_fail(pu_bootpart_force_ro(bootpart_device, 0), FALSE);

    if (!g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL)) {
        g_critical("Failed writing to boot partition!");
        return FALSE;
    }

    g_return_val_if_fail(pu_bootpart_force_ro(bootpart_device, 1), FALSE);

    return TRUE;
}

gboolean
pu_bootpart_enable(const gchar *device,
                   guint bootpart)
{
    g_return_val_if_fail(bootpart <= 2, FALSE);

    g_autofree gchar *cmd = g_strdup_printf("mmc bootpart enable %u 0 %s",
                                            bootpart, device);

    if (!g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL)) {
        g_critical("Failed %s boot partitions on %s!",
                   bootpart > 0 ? "enabling" : "disabling", device);
        return FALSE;
    }

    return TRUE;
}

/* TODO: Move lookup functions to config object or separate hashtable.c */

gchar *
pu_hash_table_lookup_string(GHashTable *hash_table,
                            const gchar *key,
                            const gchar *def)
{
    PuConfigValue *value = g_hash_table_lookup(hash_table, key);

    if (value == NULL)
        return g_strdup(def);

    return g_strdup(value->data.string);
}

gint64
pu_hash_table_lookup_int64(GHashTable *hash_table,
                           const gchar *key,
                           gint64 def)
{
    PuConfigValue *value = g_hash_table_lookup(hash_table, key);

    if (value == NULL)
        return def;

    return value->data.integer;
}

gboolean
pu_hash_table_lookup_boolean(GHashTable *hash_table,
                             const gchar *key,
                             gboolean def)
{
    PuConfigValue *value = g_hash_table_lookup(hash_table, key);

    if (value == NULL)
        return def;

    return value->data.boolean;
}

PedSector
pu_hash_table_lookup_sector(GHashTable *hash_table,
                            PedDevice *device,
                            const gchar *key,
                            PedSector def)
{
    PuConfigValue *value = g_hash_table_lookup(hash_table, key);
    PedSector sector;

    if (value == NULL)
        return def;

    if (value->type == PU_CONFIG_VALUE_TYPE_INTEGER_10 ||
        value->type == PU_CONFIG_VALUE_TYPE_INTEGER_16) {
        return value->data.integer;
    }

    if (value->type != PU_CONFIG_VALUE_TYPE_STRING ||
        !ped_unit_parse(value->data.string, device, &sector, NULL)) {
        g_warning("Failed parsing value '%s' to sectors, using default '%lld'",
                  value->data.string, def);
        return def;
    }

    return sector;
}

GList *
pu_hash_table_lookup_list(GHashTable *hash_table,
                          const gchar *key,
                          GList *def)
{
    PuConfigValue *value = g_hash_table_lookup(hash_table, key);

    if (value == NULL)
        return def;

    if (value->type != PU_CONFIG_VALUE_TYPE_SEQUENCE) {
        g_warning("Failed parsing sequence for key '%s'", key);
        return def;
    }

    return value->data.sequence;
}
