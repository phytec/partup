/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "pu-package.h"
#include "helper.h"

#define PACKAGE_FILENAME "package.partup"

static void
package_create(PackageFilesFixture *fixture,
               G_GNUC_UNUSED gconstpointer user_data)
{
    g_autoptr(GFile) package_file = NULL;
    g_assert(g_chdir(fixture->path_tmp) == 0);

    g_assert_true(pu_package_create(fixture->input_files, PACKAGE_FILENAME, FALSE, &fixture->error));
    g_assert_no_error(fixture->error);

    package_file = g_file_new_build_filename(fixture->path_tmp, PACKAGE_FILENAME, NULL);
    g_assert_true(g_file_delete(package_file, NULL, &fixture->error));
    g_assert_no_error(fixture->error);

    g_assert(g_chdir(fixture->path_test) == 0);
}

/* Create a temporary directory containing the given files. Parent directories
 * are created as needed. */
static gchar *
tmp_tree_new(const gchar * const *names)
{
    GError *error = NULL;
    gchar *path = g_dir_make_tmp("partup-XXXXXX", &error);

    g_assert_no_error(error);
    for (; *names != NULL; names++) {
        g_autofree gchar *file = g_build_filename(path, *names, NULL);
        g_autofree gchar *dir = g_path_get_dirname(file);

        g_assert_cmpint(g_mkdir_with_parents(dir, 0755), ==, 0);
        g_assert_true(g_file_set_contents(file, "", 0, &error));
        g_assert_no_error(error);
    }

    return path;
}

/* Remove the temporary directory and everything in it */
static void
tmp_tree_free(gchar *path)
{
    g_autofree gchar *cmd = g_strdup_printf("rm -rf '%s'", path);

    g_assert_true(g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL));
    g_free(path);
}

static gchar *
package_list(const gchar *package)
{
    GError *error = NULL;
    gchar *listing = NULL;
    gint wait_status;
    g_autofree gchar *cmd = g_strdup_printf("unsquashfs -ll -d squashfs-root './%s'",
                                            package);

    g_assert_true(g_spawn_command_line_sync(cmd, &listing, NULL, &wait_status,
                                            &error));
    g_assert_no_error(error);
    g_assert_true(g_spawn_check_wait_status(wait_status, &error));
    g_assert_no_error(error);

    return listing;
}

static void
package_create_preserve_path(G_GNUC_UNUSED gconstpointer user_data)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *path_test = g_get_current_dir();
    g_autofree gchar *listing = NULL;
    const gchar *names[] = { "layout.yaml", "foo/bar.txt", "foo/sibling.txt",
                             NULL };
    gchar *files[] = { "layout.yaml", "foo/bar.txt", NULL };
    gchar *path_tmp = tmp_tree_new(names);

    g_assert(g_chdir(path_tmp) == 0);
    g_assert_true(pu_package_create(files, PACKAGE_FILENAME, FALSE, &error));
    g_assert_no_error(error);

    listing = package_list(PACKAGE_FILENAME);
    g_assert_nonnull(g_strstr_len(listing, -1, "squashfs-root/foo/bar.txt"));
    g_assert_nonnull(g_strstr_len(listing, -1, "squashfs-root/layout.yaml"));
    g_assert_null(g_strstr_len(listing, -1, "sibling.txt"));

    g_assert(g_chdir(path_test) == 0);
    tmp_tree_free(path_tmp);
}

static void
package_create_dash_name(G_GNUC_UNUSED gconstpointer user_data)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *path_test = g_get_current_dir();
    g_autofree gchar *listing = NULL;
    g_autofree gchar *output_abs = NULL;
    const gchar *names[] = { "layout.yaml", "-v", NULL };
    gchar *files[] = { "layout.yaml", "-v", NULL };
    gchar *path_tmp = tmp_tree_new(names);

    /* Names starting with a dash must not be interpreted as options */
    g_assert(g_chdir(path_tmp) == 0);
    g_assert_true(pu_package_create(files, "-package.partup", FALSE, &error));
    g_assert_no_error(error);

    listing = package_list("-package.partup");
    g_assert_nonnull(g_strstr_len(listing, -1, "squashfs-root/-v"));
    g_assert_nonnull(g_strstr_len(listing, -1, "squashfs-root/layout.yaml"));

    /* Absolute output path with a leading dash in the filename */
    output_abs = g_build_filename(path_tmp, "-abs.partup", NULL);
    g_assert_true(pu_package_create(files, output_abs, FALSE, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_test(output_abs, G_FILE_TEST_IS_REGULAR));

    g_assert(g_chdir(path_test) == 0);
    tmp_tree_free(path_tmp);
}

static void
package_create_invalid(G_GNUC_UNUSED gconstpointer user_data)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *path_test = g_get_current_dir();
    g_autofree gchar *abs_file = NULL;
    const gchar *names[] = { "layout.yaml", "foo/layout.yaml", NULL };
    gchar *files_subdir_layout[] = { "foo/layout.yaml", NULL };
    gchar *files_parent[] = { "layout.yaml", "..", NULL };
    gchar *files_cwd[] = { "layout.yaml", ".", NULL };
    gchar *files_abs[] = { "layout.yaml", NULL, NULL };
    gchar *path_tmp = tmp_tree_new(names);

    g_assert(g_chdir(path_tmp) == 0);

    /* Layout configuration must be placed at the package root */
    g_assert_false(pu_package_create(files_subdir_layout, PACKAGE_FILENAME,
                                     FALSE, &error));
    g_assert_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_INVALID_PATH);
    g_clear_error(&error);

    /* Input files must be located inside the current directory */
    g_assert_false(pu_package_create(files_parent, PACKAGE_FILENAME, FALSE,
                                     &error));
    g_assert_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_INVALID_PATH);
    g_clear_error(&error);

    g_assert_false(pu_package_create(files_cwd, PACKAGE_FILENAME, FALSE,
                                     &error));
    g_assert_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_INVALID_PATH);
    g_clear_error(&error);

    abs_file = g_build_filename(path_test, "config", "simple.yaml", NULL);
    files_abs[1] = abs_file;
    g_assert_false(pu_package_create(files_abs, PACKAGE_FILENAME, FALSE,
                                     &error));
    g_assert_error(error, PU_PACKAGE_ERROR, PU_PACKAGE_ERROR_INVALID_PATH);
    g_clear_error(&error);

    g_assert(g_chdir(path_test) == 0);
    tmp_tree_free(path_tmp);
}

int
main(int argc,
     char *argv[])
{
    g_test_init(&argc, &argv, NULL);

#ifdef PARTUP_TEST_SRCDIR
    g_chdir(PARTUP_TEST_SRCDIR);
#endif

    g_test_add("/package/create", PackageFilesFixture, NULL,
               package_files_setup, package_create, package_files_teardown);
    g_test_add_data_func("/package/create/preserve-path",
                         NULL, package_create_preserve_path);
    g_test_add_data_func("/package/create/dash-name",
                         NULL, package_create_dash_name);
    g_test_add_data_func("/package/create/invalid",
                         NULL, package_create_invalid);

    return g_test_run();
}
