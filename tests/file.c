/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2024 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "pu-file.h"
#include "pu-error.h"

#define ROOT_EXT4_SIZE 262144

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

static void
test_file_get_size(void)
{
    g_autoptr(GError) error = NULL;
    goffset size = pu_file_get_size("data/root.ext4", &error);
    g_assert_no_error(error);
    g_assert_cmpuint(size, ==, ROOT_EXT4_SIZE);
}

static void
test_file_read_int64(void)
{
    g_autoptr(GError) error = NULL;
    const gchar *file_zero = "data/file-zero.txt";
    const gchar *file_integer = "data/file-integer.txt";
    const gchar *file_bad = "data/lorem.txt";
    gint64 zero = 42;
    gint64 integer = 0;

    g_assert_true(pu_file_read_int64(file_zero, &zero, &error));
    g_assert_no_error(error);
    g_assert_cmpint(zero, ==, 0);

    g_clear_error(&error);
    g_assert_true(pu_file_read_int64(file_integer, &integer, &error));
    g_assert_no_error(error);
    g_assert_cmpint(integer, ==, 67108864);

    g_clear_error(&error);
    g_assert_false(pu_file_read_int64(file_bad, &integer, &error));
    g_assert_error(error, PU_ERROR, PU_ERROR_FAILED);
}

int
main(int argc,
     char *argv[])
{
    g_test_init(&argc, &argv, NULL);

#ifdef PARTUP_TEST_SRCDIR
    g_chdir(PARTUP_TEST_SRCDIR);
#endif

    g_test_add_func("/file/file_copy", test_file_copy);
    g_test_add_func("/file/file_copy_fail", test_file_copy_fail);
    g_test_add_func("/file/file_get_size", test_file_get_size);
    g_test_add_func("/file/file_read_int64", test_file_read_int64);

    return g_test_run();
}
