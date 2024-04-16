/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#define G_LOG_DOMAIN "partup-loopdev"

#include <gio/gio.h>
#include "pu-error.h"
#include "pu-loopdev.h"
#include "pu-utils.h"

PuLoopdev *
pu_loopdev_new(gsize size,
               GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_autofree gchar *buffer = NULL;
    g_autoptr(GFileIOStream) iostream = NULL;

    PuLoopdev *loopdev = g_new0(PuLoopdev, 1);
    loopdev->file = g_file_new_tmp("partup-loopdev-XXXXXX", &iostream, error);
    if (!loopdev->file)
        return NULL;

    buffer = g_new0(gchar, size);
    if (!buffer) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Failed creating a new loopdev");
        return NULL;
    }

    if (!g_file_replace_contents(loopdev->file, buffer, size, NULL, FALSE,
                                 0, NULL, NULL, error))
        return NULL;

    return loopdev;
}

gboolean
pu_loopdev_attach(PuLoopdev *loopdev,
                  GError **error)
{
    g_return_val_if_fail(loopdev, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_autofree gchar *cmd = NULL;
    g_autofree gchar *path = NULL;

    cmd = g_strdup_printf("losetup -f --show -P %s", g_file_get_path(loopdev->file));
    if (!pu_spawn_command_line_sync_result(cmd, &path, error)) {
        return FALSE;
    }

    loopdev->device = g_strdup(g_strstrip(path));
    return TRUE;
}

gboolean
pu_loopdev_detach(PuLoopdev *loopdev,
                  GError **error)
{
    g_return_val_if_fail(loopdev, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_autofree gchar *cmd = NULL;

    cmd = g_strdup_printf("losetup -d %s", loopdev->device);
    if (!pu_spawn_command_line_sync(cmd, error))
        return FALSE;

    g_free(loopdev->device);
    return TRUE;
}

void
pu_loopdev_free(PuLoopdev *loopdev)
{
    if (!loopdev)
        return;

    g_file_delete(loopdev->file, NULL, NULL);
    g_object_unref(loopdev->file);
    g_free(loopdev);
}
