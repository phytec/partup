/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <fcntl.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <sys/stat.h>
#include <sys/types.h>
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

    for (guint i = 0; g_ptr_array_index(files, i) != NULL; i++) {
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

static gboolean
pu_package_print_dir_content(GFile *dir,
                             gboolean recursive,
                             GError **error)
{
    g_autoptr(GFileEnumerator) dir_enum = NULL;
    g_autofree gchar *dir_path = NULL;
    guint prefix_len = strlen(PU_PACKAGE_PREFIX) + 10; /* additional length of "-00000000/" */
    guint64 child_size;
    const gchar *file_attr = G_FILE_ATTRIBUTE_STANDARD_NAME ","
                             G_FILE_ATTRIBUTE_STANDARD_SIZE;

    dir_path = g_file_get_path(dir);
    pu_str_pre_remove(dir_path, prefix_len);
    if (strlen(dir_path))
        g_print("%s/\n", dir_path);
    dir_enum = g_file_enumerate_children(dir, file_attr, G_FILE_QUERY_INFO_NONE, NULL, error);
    if (!dir_enum) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Failed creating enumerator for '%s'",
                    g_file_get_path(dir));
        return FALSE;
    }

    while (TRUE) {
        GFileInfo *child_info;
        g_autofree gchar *child_path = NULL;

        if (!g_file_enumerator_iterate(dir_enum, &child_info, NULL, NULL, error)) {
            g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                        "Failed iterating file enumerator for '%s'",
                        g_file_get_path(dir));
            return FALSE;
        }

        if (!child_info)
            break;

        child_path = g_build_filename(g_file_get_path(dir), g_file_info_get_name(child_info), NULL);
        if (g_file_test(child_path, G_FILE_TEST_IS_DIR) && recursive) {
            g_autoptr(GFile) subdir = g_file_new_for_path(child_path);

            if (!pu_package_print_dir_content(subdir, TRUE, error))
                return FALSE;
        } else {
            pu_str_pre_remove(child_path, prefix_len);
            child_size = g_file_info_get_size(child_info);
            g_print("%s (%s)\n", child_path, g_format_size(child_size));
        }
    }

    return TRUE;
}

gboolean
pu_package_list_contents(const gchar *package,
                         GError **error)
{
    g_autofree gchar *mount = NULL;
    g_autoptr(GFile) dir = NULL;

    if (!g_file_test(package, G_FILE_TEST_IS_REGULAR)) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "'%s' is not a regular file", package);
        return FALSE;
    }

    mount = g_strdup_printf("%s-%08x", PU_PACKAGE_PREFIX, g_random_int());
    if (!pu_create_mount_point(g_path_get_basename(mount), error))
        return FALSE;

    if (!pu_mount(package, mount, "squashfs", "loop,ro", error))
        return FALSE;

    dir = g_file_new_for_path(mount);
    if (!pu_package_print_dir_content(dir, TRUE, error))
        return FALSE;

    if (!pu_umount(mount, error))
        return FALSE;
    if (g_rmdir(mount) < 0)
        return FALSE;

    return TRUE;
}
