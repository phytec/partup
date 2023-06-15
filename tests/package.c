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

    return g_test_run();
}
