/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "error.h"
#include "mount.h"

gchar *
pu_create_mount_point(const gchar *name,
                      GError **error)
{
    gchar *mount_point = g_build_filename(PU_MOUNT_PREFIX, name, NULL);

    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    if (g_file_test(mount_point, G_FILE_TEST_IS_DIR)) {
        g_debug("Mount point '%s' already exists", mount_point);
        return mount_point;
    }

    if (g_mkdir_with_parents(mount_point, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Failed creating mount point '%s', errno %u",
                    mount_point, errno);
        return NULL;
    }

    return mount_point;
}

gboolean
pu_mount(const gchar *source,
         const gchar *mount_point,
         GError **error)
{
    g_autofree gchar *cmd = g_strdup_printf("mount %s %s", source, mount_point);

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!g_spawn_command_line_sync(cmd, NULL, NULL, NULL, error)) {
        g_prefix_error(error, "Failed mounting '%s' to '%s': ",
                       source, mount_point);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_umount(const gchar *mount_point,
          GError **error)
{
    g_autofree gchar *cmd = g_strdup_printf("umount %s", mount_point);

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!g_spawn_command_line_sync(cmd, NULL, NULL, NULL, error)) {
        g_prefix_error(error, "Failed unmounting '%s': ", mount_point);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_device_mounted(const gchar *device)
{
    g_autofree gchar *cmd = g_strdup("mount");
    g_autofree gchar *output = NULL;

    g_spawn_command_line_sync(cmd, &output, NULL, NULL, NULL);

    return g_regex_match_simple(device, output, G_REGEX_MULTILINE, 0);
}
