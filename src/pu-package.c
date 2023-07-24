/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#define G_LOG_DOMAIN "partup-package"

#include <fcntl.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "pu-error.h"
#include "pu-package.h"
#include "pu-utils.h"

G_DEFINE_QUARK(pu-package-context-error-quark, pu_package_error)

gchar *
pu_package_get_layout_file(const gchar *pwd,
                           GError **error)
{
    g_autofree gchar *layout_file = NULL;
    g_autoptr(GFile) dir = NULL;
    g_autoptr(GFileEnumerator) dir_enum = NULL;

    dir = g_file_new_for_path(pwd);
    dir_enum = g_file_enumerate_children(dir, G_FILE_ATTRIBUTE_STANDARD_NAME,
                                         G_FILE_QUERY_INFO_NONE, NULL, error);
    if (!dir_enum) {
        g_set_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_ITER_FAILED,
                    "Failed creating enumerator for '%s'",
                    g_file_get_path(dir));
        return FALSE;
    }

    while (TRUE) {
        GFileInfo *child_info;
        g_autofree gchar *child_path = NULL;

        if (!g_file_enumerator_iterate(dir_enum, &child_info, NULL, NULL, error)) {
            g_set_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_ITER_FAILED,
                        "Failed iterating file enumerator for '%s'",
                        g_file_get_path(dir));
            return FALSE;
        }

        if (!child_info)
            break;

        child_path = g_build_filename(g_file_get_path(dir),
                                      g_file_info_get_name(child_info), NULL);

        if (g_file_test(child_path, G_FILE_TEST_IS_REGULAR) &&
            g_str_has_suffix(child_path, ".yaml")) {
            if (!layout_file) {
                layout_file = g_steal_pointer(&child_path);
            } else {
                g_set_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_MULTIPLE_LAYOUT,
                            "Multiple layout files detected: '%s' and '%s'",
                            layout_file, child_path);
                return NULL;
            }
        }
    }

    if (!layout_file) {
        g_set_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_MISSING_LAYOUT,
                    "No layout file found in '%s'", g_file_get_path(dir));
        return NULL;
    }

    return g_steal_pointer(&layout_file);
}

gboolean
pu_package_create(gchar **files,
                  const gchar *output,
                  gboolean force_overwrite,
                  GError **error)
{
    g_autofree gchar *cmd = NULL;
    g_autofree gchar *input = NULL;
    guint layout_yaml_count = 0;

    g_return_val_if_fail(files != NULL, FALSE);
    g_return_val_if_fail(g_strcmp0(output, "") > 0, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    /* Check if specified input files exist and search for required layout
     * configuration file */
    for (guint i = 0; i < g_strv_length(files); i++) {
        g_autofree gchar *file = g_strdup(files[i]);

        if (!g_file_test(file, G_FILE_TEST_EXISTS)) {
            g_set_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_NOT_FOUND,
                        "Input file '%s' does not exist", file);
            return FALSE;
        }

        if (g_str_has_suffix(file, ".yaml"))
            layout_yaml_count++;
    }

    if (!layout_yaml_count) {
        g_set_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_MISSING_LAYOUT,
                    "Package input files do not contain a layout configuration");
        return FALSE;
    }

    if (layout_yaml_count > 1) {
        g_set_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_MULTIPLE_LAYOUT,
                    "Package input files must contain only one layout configuration");
        return FALSE;
    }


    /* Check if specified output file exists and should be overwritten */
    if (g_file_test(output, G_FILE_TEST_IS_REGULAR)) {
        if (!force_overwrite) {
            g_set_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_EXISTS,
                        "Package '%s' already exists", output);
            return FALSE;
        }

        g_remove(output);
    }

    input = g_strjoinv(" ", files);
    cmd = g_strdup_printf("mksquashfs %s %s", input, output);

    if (!pu_spawn_command_line_sync(cmd, error)) {
        g_set_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_CREATION_FAILED,
                    "Failed creating package '%s': ", output);
        return FALSE;
    }

    return TRUE;
}

static gboolean
print_dir_content(GFile *dir,
                  gboolean recursive,
                  gboolean print_size,
                  GError **error)
{
    g_autoptr(GString) output = NULL;
    g_autoptr(GFileEnumerator) dir_enum = NULL;
    g_autofree gchar *dir_path = NULL;
    guint prefix_len = strlen(PU_PACKAGE_PREFIX) + 10; /* additional length of "-00000000/" */
    guint64 child_size;
    const gchar *file_attr = G_FILE_ATTRIBUTE_STANDARD_NAME ","
                             G_FILE_ATTRIBUTE_STANDARD_SIZE;

    output = g_string_new(NULL);
    dir_path = g_file_get_path(dir);
    pu_str_pre_remove(dir_path, prefix_len);
    if (strlen(dir_path))
        g_message("%s/", dir_path);
    dir_enum = g_file_enumerate_children(dir, file_attr, G_FILE_QUERY_INFO_NONE, NULL, error);
    if (!dir_enum) {
        g_set_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_ITER_FAILED,
                    "Failed creating enumerator for '%s'",
                    g_file_get_path(dir));
        return FALSE;
    }

    while (TRUE) {
        GFileInfo *child_info;
        g_autofree gchar *child_path = NULL;

        if (!g_file_enumerator_iterate(dir_enum, &child_info, NULL, NULL, error)) {
            g_set_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_ITER_FAILED,
                        "Failed iterating file enumerator for '%s'",
                        g_file_get_path(dir));
            return FALSE;
        }

        if (!child_info)
            break;

        child_path = g_build_filename(g_file_get_path(dir), g_file_info_get_name(child_info), NULL);
        if (g_file_test(child_path, G_FILE_TEST_IS_DIR) && recursive) {
            g_autoptr(GFile) subdir = g_file_new_for_path(child_path);

            if (!print_dir_content(subdir, TRUE, print_size, error))
                return FALSE;
        } else {
            pu_str_pre_remove(child_path, prefix_len);
            g_string_append(output, child_path);
            if (print_size) {
                child_size = g_file_info_get_size(child_info);
                g_string_append_printf(output, " (%s)", g_format_size(child_size));
            }
            g_message("%s", output->str);
            g_string_erase(output, 0, -1);
        }
    }

    return TRUE;
}

gboolean
pu_package_list_contents(const gchar *package,
                         gboolean print_size,
                         GError **error)
{
    g_autofree gchar *dir_basename = NULL;
    g_autofree gchar *mountpoint = NULL;
    g_autofree gchar *layout_file = NULL;
    g_autoptr(GFile) dir = NULL;

    g_return_val_if_fail(g_strcmp0(package, "") > 0, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!g_file_test(package, G_FILE_TEST_IS_REGULAR)) {
        g_set_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_NOT_FOUND,
                    "'%s' is not a regular file", package);
        return FALSE;
    }

    if (!pu_package_mount(package, &mountpoint, &layout_file, error))
        return FALSE;

    dir = g_file_new_for_path(mountpoint);
    if (!print_dir_content(dir, TRUE, print_size, error)) {
        pu_umount(mountpoint, NULL);
        return FALSE;
    }

    if (!pu_package_umount(mountpoint, error))
        return FALSE;

    return TRUE;
}

gboolean
pu_package_mount(const gchar *package,
                 gchar **mountpoint,
                 gchar **layout_file,
                 GError **error)
{
    g_return_val_if_fail(mountpoint != NULL && *mountpoint == NULL, FALSE);
    g_return_val_if_fail(layout_file != NULL && *layout_file == NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!g_file_test(package, G_FILE_TEST_IS_REGULAR)) {
        g_set_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_NOT_FOUND,
                    "'%s' is not a regular file", package);
        return FALSE;
    }

    *mountpoint = g_strdup_printf("%s-%08x", PU_PACKAGE_PREFIX, g_random_int());
    if (!pu_create_mount_point(g_path_get_basename(*mountpoint), error))
        return FALSE;

    if (!pu_mount(package, *mountpoint, "squashfs", "loop,ro", error))
        return FALSE;

    *layout_file = pu_package_get_layout_file(*mountpoint, error);
    if (*layout_file == NULL) {
        g_prefix_error(error, "Invalid package: ");
        pu_umount(*mountpoint, NULL);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_package_umount(const gchar *mountpoint,
                  GError **error)
{
    g_return_val_if_fail(g_strcmp0(mountpoint, "") > 0, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!pu_umount(mountpoint, error))
        return FALSE;

    if (g_rmdir(mountpoint) < 0) {
        g_set_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_FAILED,
                    "Failed cleaning up directory '%s'", mountpoint);
        return FALSE;
    }

    return TRUE;
}
