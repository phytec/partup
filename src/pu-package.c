/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <gio/gio.h>
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
        g_debug("Input file '%s' exists", file);
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

static gsize
calc_max_len_name(GFile *dir,
                  gboolean recursive)
{
    g_autoptr(GFileEnumerator) dir_enum = NULL;
    gsize len = 0;
    gsize max_len = 0;

    // TODO: Consider name length of this dir

    dir_enum = g_file_enumerate_children(dir, G_FILE_ATTRIBUTE_STANDARD_NAME,
                                         G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if (!dir_enum) {
        g_critical("Failed creating enumerator for '%s'", g_file_get_path(dir));
        return 0;
    }

    while (TRUE) {
        GFileInfo *child_info;
        g_autofree gchar *child_path = NULL;

        if (!g_file_enumerator_iterate(dir_enum, &child_info, NULL, NULL, NULL)) {
            g_critical("Failed iterating file enumerator for '%s'",
                       g_file_get_path(dir));
            return FALSE;
        }

        if (!child_info)
            break;

        len = g_utf8_strlen(g_file_info_get_name(child_info), -1);
        child_path = g_build_filename(g_file_get_path(dir), g_file_info_get_name(child_info), NULL);
        if (g_file_test(child_path, G_FILE_TEST_IS_DIR) && recursive) {
            g_autoptr(GFile) subdir = g_file_new_for_path(child_path);

            max_len = MAX(calc_max_len_name(subdir, TRUE), len);
        } else {
            max_len = MAX(max_len, len);
        }
    }

    return max_len;
}

static void
print_child(GFileInfo *info,
            gsize max_len)
{
    const gchar *name = g_file_info_get_name(info);
    goffset size = g_file_info_get_size(info);

    g_print("  %s%*s %s\n", name, (gint) (max_len + 4 - g_utf8_strlen(name, -1)),
            "", g_format_size(size));
}

static gboolean
pu_package_print_dir_content(GFile *dir,
                             gboolean recursive,
                             GError **error)
{
    g_autoptr(GFileEnumerator) dir_enum = NULL;
    g_autoptr(GFileInfo) dir_info = NULL;
    gsize max_len;
    const gchar *file_attr = G_FILE_ATTRIBUTE_STANDARD_NAME ","
                             G_FILE_ATTRIBUTE_STANDARD_SIZE;

    max_len = calc_max_len_name(dir, recursive);

    dir_info = g_file_query_info(dir, file_attr, G_FILE_QUERY_INFO_NONE, NULL, error);
    g_print("%s%*s %s\n", g_file_info_get_name(dir_info),
            (gint) (max_len + 6 - g_utf8_strlen(g_file_info_get_name(dir_info), -1)),
            "", g_format_size(g_file_info_get_size(dir_info)));

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
            print_child(child_info, max_len);
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
    g_debug("Temporarily mounting to %s", mount);
    if (!pu_create_mount_point(g_path_get_basename(mount), error))
        return FALSE;

    if (!pu_mount(package, mount, "squashfs", "loop,ro", error))
        return FALSE;

    dir = g_file_new_for_path(mount);
    if (!pu_package_print_dir_content(dir, TRUE, error))
        return FALSE;

    if (!pu_umount(mount, error))
        return FALSE;

    return TRUE;
}
