/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#define G_LOG_DOMAIN "partup-utils"

#include <fcntl.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <blkid.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "pu-config.h"
#include "pu-error.h"
#include "pu-glib-compat.h"
#include "pu-utils.h"

#define UDEVADM_SETTLE_TIMEOUT 10

gboolean
pu_spawn_command_line_sync(const gchar *command_line,
                           GError **error)
{
    GSpawnFlags spawn_flags;
    gchar **argv = NULL;
    gint wait_status;
    g_autofree gchar *errmsg = NULL;

    g_return_val_if_fail(command_line != NULL, FALSE);

    g_debug("Running command: %s", command_line);

    if (!g_shell_parse_argv(command_line, NULL, &argv, error))
        return FALSE;

    spawn_flags = G_SPAWN_SEARCH_PATH |
                  G_SPAWN_STDOUT_TO_DEV_NULL;
    if (!g_spawn_sync(NULL, argv, NULL, spawn_flags, NULL, NULL, NULL, &errmsg,
                      &wait_status, error)) {
        g_prefix_error(error, "Failed spawning process: ");
        g_strfreev(argv);
        return FALSE;
    }

    if (!g_spawn_check_wait_status(wait_status, error)) {
        g_prefix_error(error, "Command '%s' failed with error message: '%s': ",
                       command_line, errmsg);
        g_strfreev(argv);
        return FALSE;
    }

    g_strfreev(argv);
    return TRUE;
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

    g_debug("Extracting '%s' to '%s'", filename, dest);

    cmd = g_strdup_printf("tar -xf %s -C %s", filename, dest);

    if (!pu_spawn_command_line_sync(cmd, error)) {
        g_prefix_error(error, "Failed extracting '%s' to '%s': ", filename, dest);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_make_filesystem(const gchar *part,
                   const gchar *fstype,
                   const gchar *label,
                   const gchar *extra_args,
                   GError **error)
{
    g_autoptr(GString) cmd = NULL;

    g_return_val_if_fail(part != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    cmd = g_string_new(NULL);

    if (g_strcmp0(fstype, "fat16") == 0) {
        g_string_append(cmd, "mkfs.fat -F 16 ");
    } else if (g_strcmp0(fstype, "fat32") == 0) {
        g_string_append(cmd, "mkfs.fat -F 32 ");
    } else if (g_strcmp0(fstype, "ext2") == 0) {
        g_string_append(cmd, "mkfs.ext2 ");
    } else if (g_strcmp0(fstype, "ext3") == 0) {
        g_string_append(cmd, "mkfs.ext3 ");
    } else if (g_strcmp0(fstype, "ext4") == 0) {
        g_string_append(cmd, "mkfs.ext4 ");
    } else if (g_strcmp0(fstype, "") <= 0) {
        /* Skip formatting if no filesytem was specified */
        return TRUE;
    } else {
        g_set_error(error, PU_ERROR, PU_ERROR_UNKNOWN_FSTYPE,
                    "Unknown filesystem type '%s' for partition '%s'", fstype, part);
        return FALSE;
    }

    if (g_strcmp0(label, "") > 0) {
        if (g_regex_match_simple("^fat(16|32)$", fstype, 0, 0)) {
            g_string_append_printf(cmd, "-n \"%s\" ", label);
        } else if (g_regex_match_simple("^ext[234]$", fstype, 0, 0)) {
            g_string_append_printf(cmd, "-L \"%s\" ", label);
        }
    }

    if (g_strcmp0(extra_args, "") > 0) {
        g_string_append_printf(cmd, "%s ", extra_args);
    }

    g_string_append(cmd, part);

    if (!pu_spawn_command_line_sync(cmd->str, error)) {
        g_prefix_error(error, "Failed creating filesystem '%s' on '%s': ", fstype, part);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_set_ext_label(const gchar *part,
                 const gchar *label,
                 GError **error)
{
    g_autofree gchar *cmd = NULL;

    g_return_val_if_fail(part != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!label)
        return TRUE;

    cmd = g_strdup_printf("e2label %s \"%s\"", part, label);

    if (!pu_spawn_command_line_sync(cmd, error)) {
        g_prefix_error(error, "Failed setting label '%s' on '%s': ", label, part);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_resize_filesystem(const gchar *part,
                     GError **error)
{
    g_autofree gchar *cmd = NULL;

    g_return_val_if_fail(part != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_debug("Resizing filesystem on: %s", part);

    cmd = g_strdup_printf("resize2fs %s", part);

    if (!pu_spawn_command_line_sync(cmd, error)) {
        g_prefix_error(error, "Failed resizing filesystem on '%s': ", part);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_write_raw(const gchar *input_path,
             const gchar *output_path,
             PedDevice *device,
             PedSector input_offset,
             PedSector output_offset,
             PedSector size,
             GError **error)
{
    g_autoptr(GFile) input_file = NULL;
    g_autoptr(GFile) output_file = NULL;
    g_autoptr(GFileInputStream) input_fistream = NULL;
    g_autoptr(GFileIOStream) output_fiostream = NULL;
    GOutputStream *output_ostream;
    g_autoptr(GFileInfo) input_finfo = NULL;
    g_autoptr(GFileInfo) output_finfo = NULL;
    goffset input_size;
    gsize buffer_size;
    gsize num_read;
    gsize input_remaining;
    gssize ret;
    g_autofree guchar *buffer = NULL;

    g_return_val_if_fail(input_path != NULL, FALSE);
    g_return_val_if_fail(output_path != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_debug("Writing '%s' to '%s'", input_path, output_path);

    /* glib uses bytes not sectors */
    input_offset *= device->sector_size;
    output_offset *= device->sector_size;

    input_file = g_file_new_for_path(input_path);
    input_finfo = g_file_query_info(input_file, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                    G_FILE_QUERY_INFO_NONE, NULL, error);
    if (input_finfo == NULL)
        return FALSE;

    if (size > 0)
        input_size = size * device->sector_size;
    else
        input_size = g_file_info_get_size(input_finfo);

    if (input_offset >= input_size) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Input offset exceeds input file size");
        return FALSE;
    }

    output_file = g_file_new_for_path(output_path);

    /* Open input and output stream */
    input_fistream = g_file_read(input_file, NULL, error);
    if (input_fistream == NULL)
        return FALSE;

    output_fiostream = g_file_open_readwrite(output_file, NULL, error);
    if (output_fiostream == NULL)
        return FALSE;
    output_ostream = g_io_stream_get_output_stream(G_IO_STREAM(output_fiostream));

    if (g_input_stream_skip(G_INPUT_STREAM(input_fistream), input_offset, NULL,
                            error) < 0)
        return FALSE;

    if (!g_seekable_seek(G_SEEKABLE(output_fiostream), output_offset,
                         G_SEEK_SET, NULL, error))
        return FALSE;

    /* Begin reading and writing */
    input_remaining = input_size - input_offset;
    buffer_size = input_remaining < PED_MEBIBYTE_SIZE ? input_remaining : PED_MEBIBYTE_SIZE;
    buffer = g_new0(guchar, buffer_size);

    while (input_remaining > 0) {
        if (input_remaining < buffer_size)
            buffer_size = input_remaining;

        ret = g_input_stream_read(G_INPUT_STREAM(input_fistream), buffer,
                                  buffer_size, NULL, error);
        if (ret < 0)
            return FALSE;
        num_read = ret;

        if (g_output_stream_write(output_ostream, buffer, num_read, NULL, error) < 0)
            return FALSE;

        input_remaining -= num_read;
    }

    return TRUE;
}

gboolean
pu_has_bootpart(const gchar *device)
{
    g_autofree gchar *path = NULL;
    g_autoptr(GFile) file = NULL;

    g_return_val_if_fail(g_strcmp0(device, "") > 0, 0);

    for (guint i = 0; i <= 1; i++) {
        path = g_strdup_printf("%sboot%u", device, i);
        file = g_file_new_for_path(path);

        if (!g_file_query_exists(file, NULL))
            return FALSE;
    }

    return TRUE;
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

    res = pu_write_raw(input, bootpart_device, device,
                       input_offset, output_offset, 0, error);

    if (!pu_bootpart_force_ro(bootpart_device, 1, error))
        return FALSE;

    return res;
}

gboolean
pu_bootpart_enable(const gchar *device,
                   guint bootpart,
                   gboolean boot_ack,
                   GError **error)
{
    g_autofree gchar *cmd = NULL;

    g_return_val_if_fail(device != NULL, FALSE);
    g_return_val_if_fail(bootpart <= 2, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    cmd = g_strdup_printf("mmc bootpart enable %u %d %s", bootpart, boot_ack, device);

    return pu_spawn_command_line_sync(cmd, error);
}

gboolean
pu_partition_set_partuuid(const gchar *device,
                          guint index,
                          const gchar *partuuid,
                          GError **error)
{
    g_autofree gchar *cmd = NULL;

    g_return_val_if_fail(g_strcmp0(device, "") > 0, FALSE);
    g_return_val_if_fail(index > 0, FALSE);
    g_return_val_if_fail(g_strcmp0(partuuid, "") > 0, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    cmd = g_strdup_printf("sfdisk --part-uuid %s %d %s",
                          device, index, partuuid);

    if (!pu_spawn_command_line_sync(cmd, error)) {
        g_prefix_error(error, "Failed setting PARTUUID on %s partition %d: ",
                       device, index);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_is_drive(const gchar *device)
{
    blkid_probe pr;
    gboolean ret = FALSE;

    g_return_val_if_fail(g_strcmp0(device, "") > 0, FALSE);

    pr = blkid_new_probe_from_filename(device);
    if (pr && blkid_probe_is_wholedisk(pr))
        ret = TRUE;

    blkid_free_probe(pr);
    return ret;
}

gboolean
pu_wait_for_partitions(GError **error)
{
    g_autofree gchar *udevadm_cmd = NULL;
    udevadm_cmd = g_strdup_printf("udevadm settle --timeout %d", UDEVADM_SETTLE_TIMEOUT);

    if (!pu_spawn_command_line_sync(udevadm_cmd, error))
        return FALSE;

    return TRUE;
}

gboolean
pu_set_hwreset(const gchar *device,
               const gchar *hwreset,
               GError **error)
{
    g_autofree gchar *cmd = NULL;

    g_return_val_if_fail(g_strcmp0(device, "") > 0, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, 0);

    if (g_strcmp0(hwreset, "") <= 0)
        return TRUE;

    if (g_strcmp0(hwreset, "enable") == 0) {
        cmd = g_strdup_printf("mmc hwreset enable %s", device);
    } else if (g_strcmp0(hwreset, "disable") == 0) {
        cmd = g_strdup_printf("mmc hwreset disable %s", device);
    } else {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Invalid option for hwreset provided: %s", hwreset);
        return FALSE;
    }

    if (!pu_spawn_command_line_sync(cmd, error)) {
        if (strstr((*error)->message, "H/W Reset is already permanently")) {
            g_debug("H/W reset already permanently set");
            g_clear_error(error);
            return TRUE;
        }
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_set_bootbus(const gchar *device,
               const gchar *bootbus,
               GError **error)
{
    g_autofree gchar *cmd = NULL;

    g_return_val_if_fail(g_strcmp0(device, "") > 0, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, 0);

    if (g_strcmp0(bootbus, "") <= 0)
        return TRUE;

    cmd = g_strdup_printf("mmc bootbus set %s %s", bootbus, device);

    if (!pu_spawn_command_line_sync(cmd, error))
        return FALSE;

    return TRUE;
}

gchar *
pu_path_from_filename(const gchar *filename,
                      const gchar *prefix,
                      GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    if (g_strcmp0(filename, "") <= 0) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "filename is empty");
        return NULL;
    }

    if (g_path_is_absolute(filename)) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Filename '%s' must be relative", filename);
        return NULL;
    }

    if (g_strcmp0(prefix, "") > 0)
        return g_build_filename(prefix, filename, NULL);
    else
        return g_strdup(filename);
}

gchar *
pu_device_get_partition_path(const gchar *device,
                             guint index,
                             GError **error)
{
    g_return_val_if_fail(g_strcmp0(device, "") > 0, NULL);
    g_return_val_if_fail(index > 0, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    if (g_regex_match_simple("(mmcblk|loop)[0-9]+", device, 0, 0)) {
        return g_strdup_printf("%sp%u", device, index);
    } else if (g_regex_match_simple("sd[a-z]+", device, 0, 0)) {
        return g_strdup_printf("%s%u", device, index);
    } else {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Invalid device name '%s'", device);
        return NULL;
    }
}

gchar *
pu_device_get_partition_pattern(const gchar *device,
                                GError **error)
{
    g_return_val_if_fail(g_strcmp0(device, "") > 0, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    if (g_regex_match_simple("(mmcblk|loop)[0-9]+", device, 0, 0)) {
        return g_strdup_printf("^%s($|p[0-9]+)", device);
    } else if (g_regex_match_simple("sd[a-z]+", device, 0, 0)) {
        return g_strdup_printf("^%s($|[0-9]+)", device);
    } else {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Invalid device name '%s'", device);
        return NULL;
    }
}

gchar *
pu_str_pre_remove(gchar *string,
                  guint n)
{
    guchar *start;

    g_return_val_if_fail(string != NULL, NULL);

    if (n >= strlen(string))
        n = strlen(string);

    start = (guchar *) string;
    start += n;

    memmove(string, start, strlen((gchar *) start) + 1);

    return string;
}
