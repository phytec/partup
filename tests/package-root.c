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
package_list_contents(PackageFilesFixture *fixture,
                      G_GNUC_UNUSED gconstpointer user_data)
{
    g_autoptr(GFile) package_file = NULL;

    g_chdir(fixture->path_tmp);

    g_assert(pu_package_create(fixture->input_files, PACKAGE_FILENAME, FALSE, &fixture->error));
    g_assert_no_error(fixture->error);

    g_assert(pu_package_list_contents(PACKAGE_FILENAME, TRUE, &fixture->error));
    g_assert_no_error(fixture->error);

    package_file = g_file_new_build_filename(fixture->path_tmp, PACKAGE_FILENAME, NULL);
    g_assert_true(g_file_delete(package_file, NULL, &fixture->error));
    g_assert_no_error(fixture->error);

    g_chdir(fixture->path_test);
}

static void
package_mount(PackageFilesFixture *fixture,
              G_GNUC_UNUSED gconstpointer user_data)
{
    g_autoptr(GFile) package_file = NULL;
    g_autofree gchar *mountpoint = NULL;
    g_autofree gchar *config_path = NULL;

    g_chdir(fixture->path_tmp);

    g_assert(pu_package_create(fixture->input_files, PACKAGE_FILENAME, FALSE, &fixture->error));
    g_assert_no_error(fixture->error);

    g_assert(pu_package_mount(PACKAGE_FILENAME, &mountpoint, &config_path, &fixture->error));
    g_assert(g_file_test(mountpoint, G_FILE_TEST_IS_DIR));
    g_assert(g_file_test(config_path, G_FILE_TEST_IS_REGULAR));

    for (guint i = 0; i < g_strv_length(fixture->input_files); i++)
        g_assert(g_file_test(fixture->input_files[i], G_FILE_TEST_IS_REGULAR));

    g_assert(pu_package_umount(mountpoint, &fixture->error));
    g_assert_no_error(fixture->error);

    package_file = g_file_new_build_filename(fixture->path_tmp, PACKAGE_FILENAME, NULL);
    g_assert_true(g_file_delete(package_file, NULL, &fixture->error));
    g_assert_no_error(fixture->error);

    g_chdir(fixture->path_test);
}

int
main(int argc,
     char *argv[])
{
    /* Skip tests when not run as root */
    if (getuid() != 0)
        return 77;

    g_test_init(&argc, &argv, NULL);

#ifdef PARTUP_TEST_SRCDIR
    g_chdir(PARTUP_TEST_SRCDIR);
#endif

    g_test_add("/package/list-contents", PackageFilesFixture, NULL,
               package_files_setup, package_list_contents, package_files_teardown);
    g_test_add("/package/mount", PackageFilesFixture, NULL,
               package_files_setup, package_mount, package_files_teardown);

    return g_test_run();
}
