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
#include "error.h"
#include "utils.h"

static gboolean
pu_spawn_command_line_sync(const gchar *command_line,
                           GError **error)
{
    gboolean res;
    GSpawnFlags spawn_flags;
    gchar **argv = NULL;

    g_return_val_if_fail(command_line != NULL, FALSE);

    if (!g_shell_parse_argv(command_line, NULL, &argv, error))
        return FALSE;

    spawn_flags = G_SPAWN_SEARCH_PATH |
                  G_SPAWN_STDOUT_TO_DEV_NULL |
                  G_SPAWN_STDERR_TO_DEV_NULL;
    res = g_spawn_sync(NULL, argv, NULL, spawn_flags, NULL, NULL, NULL, NULL, NULL, error);
    g_strfreev(argv);

    return res;
}

gboolean
pu_file_copy(const gchar *src,
             const gchar *dest,
             GError **error)
{
    g_autoptr(GFile) in = NULL;
    g_autofree gchar *out_path = NULL;
    g_autoptr(GFile) out = NULL;

    g_return_val_if_fail(src != NULL, FALSE);
    g_return_val_if_fail(dest != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    in = g_file_new_for_path(src);
    out_path = g_build_filename(dest, g_path_get_basename(src), NULL);
    out = g_file_new_for_path(out_path);

    return g_file_copy(in, out, G_FILE_COPY_NONE, NULL, NULL, NULL, error);
}

gboolean
pu_archive_extract(const gchar *filename,
                   const gchar *dest,
                   GError **error)
{
    g_autofree gchar *cmd = NULL;

    g_return_val_if_fail(filename != NULL, FALSE);
    g_return_val_if_fail(dest != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    cmd = g_strdup_printf("tar -xf %s -C %s", filename, dest);

    return pu_spawn_command_line_sync(cmd, error);
}

gboolean
pu_make_filesystem(const gchar *part,
                   const gchar *fstype,
                   GError **error)
{
    g_autoptr(GString) cmd = NULL;

    g_return_val_if_fail(part != NULL, FALSE);
    g_return_val_if_fail(fstype != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    cmd = g_string_new(NULL);

    if (g_strcmp0(fstype, "fat32") == 0) {
        g_string_append(cmd, "mkfs.vfat ");
    } else if (g_strcmp0(fstype, "ext2") == 0) {
        g_string_append(cmd, "mkfs.ext2 ");
    } else if (g_strcmp0(fstype, "ext3") == 0) {
        g_string_append(cmd, "mkfs.ext3 ");
    } else if (g_strcmp0(fstype, "ext4") == 0) {
        g_string_append(cmd, "mkfs.ext4 ");
    } else {
        g_set_error(error, PU_ERROR, PU_ERROR_UNKNOWN_FSTYPE,
                    "Unknown filesystem type '%s' for partition '%s'", fstype, part);
        return FALSE;
    }
    g_string_append(cmd, part);

    return pu_spawn_command_line_sync(cmd->str, error);
}

gboolean
pu_write_raw(const gchar *input,
             const gchar *output,
             PedSector block_size,
             PedSector input_offset,
             PedSector output_offset,
             GError **error)
{
    g_autofree gchar *cmd = NULL;

    g_return_val_if_fail(input != NULL, FALSE);
    g_return_val_if_fail(output != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    cmd = g_strdup_printf("dd if=%s of=%s bs=%lld skip=%lld seek=%lld",
                          input, output, block_size, input_offset, output_offset);

    return pu_spawn_command_line_sync(cmd, error);
}

static gboolean
pu_bootpart_force_ro(const gchar *bootpart,
                     gboolean read_only,
                     GError **error)
{
    g_autofree gchar *path = NULL;
    FILE *file = NULL;

    path = g_build_filename("/sys/class/block", g_path_get_basename(bootpart),
                            "force_ro", NULL);
    file = g_fopen(path, "w");

    if (file == NULL) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "Failed opening file '%s'", path);
        return FALSE;
    }

    if (!g_fprintf(file, "%d", read_only)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "Failed writing '%d' to '%s'!", read_only, path);
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
                      PedSector output_offset,
                      GError **error)
{
    gboolean res;
    g_autofree gchar *bootpart_device = NULL;

    g_return_val_if_fail(input != NULL, FALSE);
    g_return_val_if_fail(device != NULL, FALSE);
    g_return_val_if_fail(bootpart <= 1, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    bootpart_device = g_strdup_printf("%sboot%d", device->path, bootpart);

    if (!pu_bootpart_force_ro(bootpart_device, 0, error))
        return FALSE;

    res = pu_write_raw(input, bootpart_device, device->sector_size,
                       input_offset, output_offset, error);

    if (!pu_bootpart_force_ro(bootpart_device, 1, error))
        return FALSE;

    return res;
}

gboolean
pu_bootpart_enable(const gchar *device,
                   guint bootpart,
                   GError **error)
{
    g_autofree gchar *cmd = NULL;

    g_return_val_if_fail(device != NULL, FALSE);
    g_return_val_if_fail(bootpart <= 2, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    cmd = g_strdup_printf("mmc bootpart enable %u 0 %s", bootpart, device);

    return pu_spawn_command_line_sync(cmd, error);
}
