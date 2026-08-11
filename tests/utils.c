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

static void
test_archive_extract(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *dest = NULL;
    g_autofree gchar *lorem_file = NULL;
    g_autofree gchar *ipsum_file = NULL;
    g_autofree gchar *dolor_file = NULL;
    g_autofree gchar *foo_dir = NULL;
    g_autofree gchar *foo_file = NULL;
    g_autofree gchar *bar_dir = NULL;
    g_autofree gchar *bar_file = NULL;
    g_autofree gchar *baz_dir = NULL;
    g_autofree gchar *baz_file = NULL;
    g_autoptr(GList) exclude = NULL;
    g_autoptr(GList) list_ba = NULL;
    g_autoptr(GList) list_notfound = NULL;
    g_autoptr(GList) only = NULL;

    dest = g_dir_make_tmp("partup-XXXXXX", &error);
    g_assert_no_error(error);

    lorem_file = g_build_filename(dest, "lorem.txt", NULL);
    ipsum_file = g_build_filename(dest, "ipsum.txt", NULL);
    dolor_file = g_build_filename(dest, "dolor.txt", NULL);

    exclude = g_list_prepend(exclude, "lorem.txt");
    exclude = g_list_prepend(exclude, "ipsum.txt");

    only = g_list_prepend(only, "ipsum.txt");
    only = g_list_prepend(only, "dolor.txt");

    list_ba = g_list_prepend(list_ba, "ba*");
    list_notfound = g_list_prepend(list_notfound, "notfound*");

    foo_dir = g_build_filename(dest, "foo", NULL);
    foo_file = g_build_filename(dest, "foo/foo.txt", NULL);
    bar_dir = g_build_filename(dest, "bar", NULL);
    bar_file = g_build_filename(dest, "bar/bar.cfg", NULL);
    baz_dir = g_build_filename(dest, "baz", NULL);
    baz_file = g_build_filename(dest, "baz/baz.yaml", NULL);

    /* Extract all */
    g_assert_true(pu_archive_extract("data/lorem.tar", dest, NULL, NULL, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_test(lorem_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(ipsum_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(dolor_file, G_FILE_TEST_IS_REGULAR));
    g_assert_cmpint(g_remove(lorem_file), ==, 0);
    g_assert_cmpint(g_remove(ipsum_file), ==, 0);
    g_assert_cmpint(g_remove(dolor_file), ==, 0);

    /* Extract all excluding two */
    g_assert_true(pu_archive_extract("data/lorem.tar", dest, exclude, NULL, &error));
    g_assert_no_error(error);
    g_assert_false(g_file_test(lorem_file, G_FILE_TEST_IS_REGULAR));
    g_assert_false(g_file_test(ipsum_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(dolor_file, G_FILE_TEST_IS_REGULAR));
    g_assert_cmpint(g_remove(dolor_file), ==, 0);

    /* Extract only two */
    g_assert_true(pu_archive_extract("data/lorem.tar", dest, NULL, only, &error));
    g_assert_no_error(error);
    g_assert_false(g_file_test(lorem_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(ipsum_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(dolor_file, G_FILE_TEST_IS_REGULAR));
    g_assert_cmpint(g_remove(ipsum_file), ==, 0);
    g_assert_cmpint(g_remove(dolor_file), ==, 0);

    /* Extract only two excluding two */
    g_assert_true(pu_archive_extract("data/lorem.tar", dest, exclude, only, &error));
    g_assert_no_error(error);
    g_assert_false(g_file_test(lorem_file, G_FILE_TEST_IS_REGULAR));
    g_assert_false(g_file_test(ipsum_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(dolor_file, G_FILE_TEST_IS_REGULAR));
    g_assert_cmpint(g_remove(dolor_file), ==, 0);

    /* Extract all excluding "ba*" */
    g_assert_true(pu_archive_extract("data/foobar.tar", dest, list_ba, NULL, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_test(foo_dir, G_FILE_TEST_IS_DIR));
    g_assert_true(g_file_test(foo_file, G_FILE_TEST_IS_REGULAR));
    g_assert_false(g_file_test(bar_dir, G_FILE_TEST_IS_DIR));
    g_assert_false(g_file_test(bar_file, G_FILE_TEST_IS_REGULAR));
    g_assert_false(g_file_test(baz_dir, G_FILE_TEST_IS_DIR));
    g_assert_false(g_file_test(baz_file, G_FILE_TEST_IS_REGULAR));
    g_assert_cmpint(g_remove(foo_file), ==, 0);
    g_assert_cmpint(g_rmdir(foo_dir), ==, 0);

    /* Extract only "ba*" (includes wildcards) */
    g_assert_true(pu_archive_extract("data/foobar.tar", dest, NULL, list_ba, &error));
    g_assert_no_error(error);
    g_assert_false(g_file_test(foo_dir, G_FILE_TEST_IS_DIR));
    g_assert_false(g_file_test(foo_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(bar_dir, G_FILE_TEST_IS_DIR));
    g_assert_true(g_file_test(bar_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(baz_dir, G_FILE_TEST_IS_DIR));
    g_assert_true(g_file_test(baz_file, G_FILE_TEST_IS_REGULAR));
    g_assert_cmpint(g_remove(bar_file), ==, 0);
    g_assert_cmpint(g_rmdir(bar_dir), ==, 0);
    g_assert_cmpint(g_remove(baz_file), ==, 0);
    g_assert_cmpint(g_rmdir(baz_dir), ==, 0);

    /* Member not found in archive */
    g_assert_false(pu_archive_extract("data/foobar.tar", dest, NULL, list_notfound, &error));
    g_assert_error(error, G_SPAWN_EXIT_ERROR, 2);

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
                  "test", NULL, &fixture->error));
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
test_set_ext_label(EmptyFileFixture *fixture,
                   G_GNUC_UNUSED gconstpointer user_data)
{
    g_autofree gchar *cmd = NULL;
    g_autofree gchar *output = NULL;
    gint wait_status;

    g_assert_true(pu_make_filesystem(g_file_get_path(fixture->file), "ext4",
                  "", NULL, &fixture->error));
    g_assert_no_error(fixture->error);

    g_assert_true(pu_set_ext_label(g_file_get_path(fixture->file), "test",
                  &fixture->error));
    g_assert_no_error(fixture->error);

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

static void
test_is_ext234_image(void)
{
    g_assert_true(pu_is_ext234_image("data/root.ext4"));
    g_assert_false(pu_is_ext234_image("data/random.bin"));
    g_assert_false(pu_is_ext234_image("data/file-zero.txt"));
    g_assert_false(pu_is_ext234_image("data/file-integer.txt"));
    g_assert_false(pu_is_ext234_image("data/lorem.tar"));
    g_assert_false(pu_is_ext234_image("data/lorem.txt"));
}

#if 0
static void
test_hash_table_intersect(void)
{
    g_autoptr(GHashTable) a = NULL;
    g_autoptr(GHashTable) b = NULL;
    g_autoptr(GHashTable) intersect = NULL;

    a = g_hash_table_new(g_str_hash, g_str_equal);
    b = g_hash_table_new(g_str_hash, g_str_equal);

    g_hash_table_add(a, "foo");
    g_hash_table_add(a, "bar");
    g_hash_table_add(a, "baz");
    g_hash_table_add(b, "baz");
    g_hash_table_add(b, "buzzer");

    /* NULL inputs return an empty (non-NULL) set. */
    intersect = pu_hash_table_intersect(NULL, NULL);
    g_assert_nonnull(intersect);
    g_assert_cmpuint(g_hash_table_size(intersect), ==, 0);
    g_hash_table_destroy(intersect);

    intersect = pu_hash_table_intersect(a, NULL);
    g_assert_nonnull(intersect);
    g_assert_cmpuint(g_hash_table_size(intersect), ==, 0);
    g_hash_table_destroy(intersect);

    intersect = pu_hash_table_intersect(NULL, b);
    g_assert_nonnull(intersect);
    g_assert_cmpuint(g_hash_table_size(intersect), ==, 0);
    g_hash_table_destroy(intersect);

    /* Real intersection: only "baz" is common. */
    intersect = pu_hash_table_intersect(a, b);
    g_assert_nonnull(intersect);
    g_assert_cmpuint(g_hash_table_size(intersect), ==, 1);
    g_assert_true(g_hash_table_contains(intersect, "baz"));
    g_assert_false(g_hash_table_contains(intersect, "foo"));
    g_assert_false(g_hash_table_contains(intersect, "buzzer"));
}
#endif

static void
test_remove_recursive_intersect(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree GList *exclude = NULL;
    g_autofree GList *only = NULL;
    g_autofree gchar *dest = NULL;
    g_autofree gchar *all_dir = NULL;
    g_autofree gchar *empty_dir = NULL;
    g_autofree gchar *nested_dir = NULL;
    g_autofree gchar *nested_file = NULL;
    g_autofree gchar *foo_file = NULL;
    g_autofree gchar *foobar_file = NULL;
    g_autofree gchar *foobarbuz1_file = NULL;
    g_autofree gchar *foobarbuz2_file = NULL;

    dest = g_dir_make_tmp("partup-XXXXXX", &error);
    g_assert_no_error(error);

    all_dir = g_build_filename(dest, "*", NULL);
    empty_dir = g_build_filename(dest, "empty", NULL);
    nested_dir = g_build_filename(dest, "nested", "one", "two", "three", NULL);
    nested_file = g_build_filename(dest, "nested", "one", "two", "three", "four.txt", NULL);
    foo_file = g_build_filename(dest, "foo", "test.c", NULL);
    foobar_file = g_build_filename(dest, "foo", "bar", "settings.cfg", NULL);
    foobarbuz1_file = g_build_filename(dest, "foo", "bar", "buz", "buzzer.yaml", NULL);
    foobarbuz2_file = g_build_filename(dest, "foo", "bar", "buz", "dozzer.yaml", NULL);

    /* NULL checks */
    g_assert_true(pu_archive_extract("data/dir-struct.tar", dest, NULL, NULL, &error));
    g_assert_no_error(error);

    g_assert_true(pu_remove_recursive_intersect(dest, NULL, NULL, &error));
    g_assert_true(g_file_test(empty_dir, G_FILE_TEST_IS_DIR));
    g_assert_true(g_file_test(nested_dir, G_FILE_TEST_IS_DIR));
    g_assert_true(g_file_test(nested_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(foo_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(foobar_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(foobarbuz1_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(foobarbuz2_file, G_FILE_TEST_IS_REGULAR));

    /* Exclude all */
    g_assert_true(pu_archive_extract("data/dir-struct.tar", dest, NULL, NULL, &error));
    g_assert_no_error(error);

    g_list_free(exclude);
    exclude = NULL;
    exclude = g_list_prepend(exclude, all_dir);
    g_list_free(only);
    only = NULL;
    g_assert_true(pu_remove_recursive_intersect(dest, exclude, only, &error));
    g_assert_false(g_file_test(empty_dir, G_FILE_TEST_IS_DIR));
    g_assert_false(g_file_test(nested_dir, G_FILE_TEST_IS_DIR));
    g_assert_false(g_file_test(nested_file, G_FILE_TEST_IS_REGULAR));
    g_assert_false(g_file_test(foo_file, G_FILE_TEST_IS_REGULAR));
    g_assert_false(g_file_test(foobar_file, G_FILE_TEST_IS_REGULAR));
    g_assert_false(g_file_test(foobarbuz1_file, G_FILE_TEST_IS_REGULAR));
    g_assert_false(g_file_test(foobarbuz2_file, G_FILE_TEST_IS_REGULAR));

    /* Preserve all */
    g_assert_true(pu_archive_extract("data/dir-struct.tar", dest, NULL, NULL, &error));
    g_assert_no_error(error);

    g_list_free(exclude);
    exclude = NULL;
    g_list_free(only);
    only = NULL;
    only = g_list_prepend(only, all_dir);
    g_assert_true(pu_remove_recursive_intersect(dest, exclude, only, &error));
    g_assert_true(g_file_test(empty_dir, G_FILE_TEST_IS_DIR));
    g_assert_true(g_file_test(nested_dir, G_FILE_TEST_IS_DIR));
    g_assert_true(g_file_test(nested_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(foo_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(foobar_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(foobarbuz1_file, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(foobarbuz2_file, G_FILE_TEST_IS_REGULAR));
}

int
main(int argc,
     char *argv[])
{
    g_test_init(&argc, &argv, NULL);

#ifdef PARTUP_TEST_SRCDIR
    g_chdir(PARTUP_TEST_SRCDIR);
#endif

    g_test_add_func("/utils/archive_extract", test_archive_extract);
    g_test_add("/utils/make_filesystem", EmptyFileFixture, "file", empty_file_set_up,
               test_make_filesystem, empty_file_tear_down);
    g_test_add("/utils/set_ext_label", EmptyFileFixture, "file", empty_file_set_up,
               test_set_ext_label, empty_file_tear_down);
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
    g_test_add_func("/utils/str_pre_remove", test_str_pre_remove);
    g_test_add_func("/utils/device_get_partition_pattern", test_device_get_partition_pattern);
    g_test_add_func("/utils/is_ext234_image", test_is_ext234_image);
    //g_test_add_func("/utils/hash_table_intersect", test_hash_table_intersect);
    g_test_add_func("/utils/remove_recursive_intersect", test_remove_recursive_intersect);

    return g_test_run();
}
