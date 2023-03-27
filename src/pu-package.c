/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include "pu-error.h"
#include "pu-package.h"
#include "pu-utils.h"

gboolean
pu_package_create(GPtrArray *files,
                  const gchar *output,
                  GError **error)
{
    g_autofree gchar *cmd = NULL;
    g_autofree gchar *input = NULL;

    g_return_val_if_fail(files != NULL, FALSE);
    g_return_val_if_fail(g_strcmp0(output, "") > 0, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (g_file_test(output, G_FILE_TEST_IS_REGULAR)) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Package already exists");
        return FALSE;
    }

    for (guint i = 0; i < files->len; i++) {
        g_autofree gchar *file = g_strdup(g_ptr_array_index(files, i));
        if (!g_file_test(file, G_FILE_TEST_EXISTS)) {
            g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                        "Input file '%s' does not exist", file);
            return FALSE;
        }
    }

    input = g_strjoinv(" ", (gchar **) files->pdata);
    cmd = g_strdup_printf("mksquashfs %s %s", input, output);

    if (!pu_spawn_command_line_sync(cmd, error)) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Failed creating package '%s': ", output);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_package_mount(const gchar *package,
                 const gchar *mount,
                 GError **error)
{
    g_autofree gchar *cmd = NULL;

    g_return_val_if_fail(g_strcmp0(package, "") > 0, FALSE);
    g_return_val_if_fail(g_strcmp0(mount, "") > 0, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    cmd = g_strdup_printf("mount -t squashfs -o loop %s %s", package, mount);

    if (!pu_spawn_command_line_sync(cmd, error)) {
        g_prefix_error(error, "Failed mounting package '%s' to '%s': ",
                       package, mount);
        return FALSE;
    }

    return TRUE;
}
