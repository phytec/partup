/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "pu-utils.h"
#include "pu-error.h"

static void
test_file_copy(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *dest = NULL;
    g_autofree gchar *out_file = NULL;
    const gchar *source = "data/lorem.txt";

    dest = g_dir_make_tmp("partup-XXXXXX", &error);
    g_assert_no_error(error);

    out_file = g_build_filename(dest, "lorem.txt", NULL);

    g_assert_true(pu_file_copy(source, dest, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_test(out_file, G_FILE_TEST_IS_REGULAR));

    g_assert_cmpint(g_remove(out_file), ==, 0);
    g_assert_cmpint(g_rmdir(dest), ==, 0);
}

static void
test_file_copy_fail(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *dest = NULL;
    g_autofree gchar *out_file = NULL;
    const gchar *source = "data/thisfilereallydoesnotexist";

    dest = g_dir_make_tmp("partup-XXXXXX", &error);
    g_assert_no_error(error);

    g_assert_false(pu_file_copy(source, dest, &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);

    g_assert_cmpint(g_rmdir(dest), ==, 0);
}

int
main(int argc,
     char *argv[])
{
    g_test_init(&argc, &argv, NULL);

#ifdef PARTUP_TEST_SRCDIR
    g_chdir(PARTUP_TEST_SRCDIR);
#endif

    g_test_add_func("/utils/file_copy", test_file_copy);
    g_test_add_func("/utils/file_copy_fail", test_file_copy_fail);

    return g_test_run();
}
