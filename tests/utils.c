/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "helper.h"
#include "pu-glib-compat.h"
#include "pu-utils.h"
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
test_archive_extract(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *dest = NULL;
    g_autofree gchar *out_file = NULL;
    const gchar *source = "data/lorem.tar";

    dest = g_dir_make_tmp("partup-XXXXXX", &error);
    g_assert_no_error(error);

    out_file = g_build_filename(dest, "lorem.txt", NULL);

    g_assert_true(pu_archive_extract(source, dest, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_test(out_file, G_FILE_TEST_IS_REGULAR));

    g_assert_cmpint(g_remove(out_file), ==, 0);
    g_assert_cmpint(g_rmdir(dest), ==, 0);
}

static void
test_make_filesystem(EmptyFileFixture *fixture,
                     G_GNUC_UNUSED gconstpointer user_data)
{
    g_autofree gchar *cmd = NULL;
    g_autofree gchar *output = NULL;
    gint wait_status;

    g_assert_true(pu_make_filesystem(g_file_get_path(fixture->file), "ext4",
                  "test", &fixture->error));
    g_assert_no_error(fixture->error);

    cmd = g_strdup_printf("blkid -o value -s TYPE %s", g_file_get_path(fixture->file));
    g_assert_true(g_spawn_command_line_sync(cmd, &output, NULL,
                                            &wait_status, &fixture->error));
    g_assert_true(g_spawn_check_wait_status(wait_status, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_nonnull(strstr(output, "ext4"));

    cmd = g_strdup_printf("blkid -o value -s LABEL %s", g_file_get_path(fixture->file));
    g_assert_true(g_spawn_command_line_sync(cmd, &output, NULL,
                                            &wait_status, &fixture->error));
    g_assert_true(g_spawn_check_wait_status(wait_status, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_nonnull(strstr(output, "test"));
}

static void
test_write_raw(EmptyFileFixture *fixture,
               G_GNUC_UNUSED gconstpointer user_data)
{
    g_autofree gchar *cmd = NULL;
    g_autofree gchar *output = NULL;
    gint wait_status;
    PedDevice device;
    device.sector_size = 512;

    g_assert_true(pu_write_raw("data/root.ext4", g_file_get_path(fixture->file),
                  &device, 0, 0, 0, &fixture->error));
    g_assert_no_error(fixture->error);

    cmd = g_strdup_printf("blkid -o value -s TYPE %s", g_file_get_path(fixture->file));
    g_assert_true(g_spawn_command_line_sync(cmd, &output, NULL,
                                            &wait_status, &fixture->error));
    g_assert_true(g_spawn_check_wait_status(wait_status, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_nonnull(strstr(output, "ext4"));
}

static void
test_write_raw_input_offset(EmptyFileFixture *fixture,
                            G_GNUC_UNUSED gconstpointer user_data)
{
    PedDevice device;
    device.sector_size = 512;
    g_assert_true(pu_write_raw("data/root.ext4", g_file_get_path(fixture->file),
                  &device, 2, 0, 0, &fixture->error));
    g_assert_no_error(fixture->error);
}

static void
test_path_from_filename(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *path = pu_path_from_filename("lorem.txt", "data", &error);
    g_assert_no_error(error);
    g_assert_cmpstr("data/lorem.txt", ==, path);
}

static void
test_path_from_filename_empty(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *path = pu_path_from_filename("", "data", &error);
    g_assert_error(error, PU_ERROR, PU_ERROR_FAILED);
    g_assert_null(path);
}

static void
test_device_get_partition_path_mmc(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *path = pu_device_get_partition_path("/dev/mmcblk0", 1, &error);
    g_assert_no_error(error);
    g_assert_cmpstr("/dev/mmcblk0p1", ==, path);
}

static void
test_device_get_partition_path_sd(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *path = pu_device_get_partition_path("/dev/sda", 3, &error);
    g_assert_no_error(error);
    g_assert_cmpstr("/dev/sda3", ==, path);
}

static void
test_device_get_partition_path_fail(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *path = pu_device_get_partition_path("/dev/null", 3, &error);
    g_assert_error(error, PU_ERROR, PU_ERROR_FAILED);
    g_assert_null(path);
}

static void
test_get_file_size(void)
{
    g_autoptr(GError) error = NULL;
    goffset size = pu_get_file_size("data/root.ext4", &error);
    g_assert_no_error(error);
    g_assert_cmpuint(size, ==, ROOT_EXT4_SIZE);
}

static void
test_str_pre_remove(void)
{
    g_autofree gchar *in = g_strdup("partup");

    g_assert_cmpstr(pu_str_pre_remove(in, 4), ==, "up");
    g_assert_cmpstr(in, ==, "up");
}

static void
test_device_get_partition_pattern(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *pattern;

    pattern = pu_device_get_partition_pattern("/dev/mmcblk0", &error);
    g_assert_no_error(error);
    g_assert_true(g_regex_match_simple(pattern, "/dev/mmcblk0", 0, 0));
    g_assert_true(g_regex_match_simple(pattern, "/dev/mmcblk0p1", 0, 0));
    g_free(pattern);

    pattern = pu_device_get_partition_pattern("/dev/loop1", &error);
    g_assert_no_error(error);
    g_assert_true(g_regex_match_simple(pattern, "/dev/loop1p1", 0, 0));
    g_assert_false(g_regex_match_simple(pattern, "/dev/loop10", 0, 0));
    g_free(pattern);

    pattern = pu_device_get_partition_pattern("/dev/sda", &error);
    g_assert_no_error(error);
    g_assert_true(g_regex_match_simple(pattern, "/dev/sda1", 0, 0));
    g_assert_false(g_regex_match_simple(pattern, "/dev/sdb1", 0, 0));
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
    g_test_add_func("/utils/archive_extract", test_archive_extract);
    g_test_add("/utils/make_filesystem", EmptyFileFixture, "file", empty_file_set_up,
               test_make_filesystem, empty_file_tear_down);
    g_test_add("/utils/write_raw", EmptyFileFixture, "file", empty_file_set_up,
               test_write_raw, empty_file_tear_down);
    g_test_add("/utils/write_raw_input_offset", EmptyFileFixture, "file", empty_file_set_up,
               test_write_raw_input_offset, empty_file_tear_down);
    g_test_add_func("/utils/path_from_filename", test_path_from_filename);
    g_test_add_func("/utils/path_from_filename_empty", test_path_from_filename_empty);
    g_test_add_func("/utils/device_get_partition_path_mmc",
                    test_device_get_partition_path_mmc);
    g_test_add_func("/utils/device_get_partition_path_sd",
                    test_device_get_partition_path_sd);
    g_test_add_func("/utils/device_get_partition_path_fail",
                    test_device_get_partition_path_fail);
    g_test_add_func("/utils/get_file_size",
                    test_get_file_size);
    g_test_add_func("/utils/str_pre_remove", test_str_pre_remove);
    g_test_add_func("/utils/device_get_partition_pattern", test_device_get_partition_pattern);

    return g_test_run();
}
