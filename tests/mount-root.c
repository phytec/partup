/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "helper.h"
#include "pu-error.h"
#include "pu-glib-compat.h"
#include "pu-mount.h"

#define MOUNT_POINT_BASENAME "partition"
#define MOUNT_POINT          PU_MOUNT_PREFIX "/" MOUNT_POINT_BASENAME
#define MOUNT_SOURCE         "data/root.ext4"

static void
test_create_mount_point(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *mount_point;

    mount_point = pu_create_mount_point(MOUNT_POINT_BASENAME, &error);
    g_assert_nonnull(mount_point);
    g_assert_cmpstr(mount_point, ==, MOUNT_POINT);
    g_assert_no_error(error);
    g_assert_true(g_file_test(mount_point, G_FILE_TEST_IS_DIR));
    g_assert_cmpint(g_rmdir(mount_point), ==, 0);
}

static void
test_mount(CopyFileFixture *fixture,
           G_GNUC_UNUSED gconstpointer user_data)
{
    g_autofree gchar *cmd = NULL;
    g_autofree gchar *mount_point = NULL;
    gint wait_status;

    mount_point = g_dir_make_tmp("partup-XXXXXX", &fixture->error);
    g_assert_no_error(fixture->error);

    g_assert_true(pu_mount(g_file_get_path(fixture->file), mount_point,
                           NULL, NULL, &fixture->error));
    g_assert_no_error(fixture->error);

    cmd = g_strdup_printf("umount %s", mount_point);
    g_assert_true(g_spawn_command_line_sync(cmd, NULL, NULL, &wait_status, &fixture->error));
    g_assert_true(g_spawn_check_wait_status(wait_status, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_cmpint(g_rmdir(mount_point), ==, 0);
}

static void
test_umount(CopyFileFixture *fixture,
            G_GNUC_UNUSED gconstpointer user_data)
{
    g_autofree gchar *mount_point = NULL;
    g_autofree gchar *cmd = NULL;
    gint wait_status;

    mount_point = g_dir_make_tmp("partup-XXXXXX", &fixture->error);
    g_assert_no_error(fixture->error);

    cmd = g_strdup_printf("mount %s %s", g_file_get_path(fixture->file),
                          mount_point);

    g_assert_true(g_spawn_command_line_sync(cmd, NULL, NULL, &wait_status, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_true(g_spawn_check_wait_status(wait_status, &fixture->error));
    g_assert_no_error(fixture->error);

    g_assert_true(pu_umount(mount_point, &fixture->error));
    g_assert_no_error(fixture->error);

    g_assert_cmpint(g_rmdir(mount_point), ==, 0);
}

static void
test_device_mounted_false(EmptyDeviceFixture *fixture,
                          G_GNUC_UNUSED gconstpointer user_data)
{
    gboolean is_mounted;

    /* Test unpartitioned device */
    g_assert_true(pu_device_mounted(fixture->loop_dev, &is_mounted, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_false(is_mounted);

    create_partition(fixture->loop_dev, &fixture->error);
    g_assert_no_error(fixture->error);

    /* Test partitioned device */
    g_assert_true(pu_device_mounted(fixture->loop_dev, &is_mounted, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_false(is_mounted);
}

static void
test_device_mounted_true(EmptyDeviceFixture *fixture,
                         G_GNUC_UNUSED gconstpointer user_data)
{
    gboolean is_mounted;
    g_autofree gchar *mount_point = NULL;

    mount_point = g_dir_make_tmp("partup-XXXXXX", &fixture->error);
    g_assert_no_error(fixture->error);

    create_partition(fixture->loop_dev, &fixture->error);
    g_assert_no_error(fixture->error);

    g_assert_true(pu_mount(g_strdup_printf("%sp1", fixture->loop_dev),
                           mount_point, NULL, NULL, &fixture->error));
    g_assert_no_error(fixture->error);

    g_assert_true(pu_device_mounted(fixture->loop_dev, &is_mounted, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_true(is_mounted);

    g_assert_true(pu_umount(mount_point, &fixture->error));
    g_assert_no_error(fixture->error);

    g_assert_cmpint(g_rmdir(mount_point), ==, 0);
}

static void
test_umount_all(EmptyDeviceFixture *fixture,
                G_GNUC_UNUSED gconstpointer user_data)
{
    gboolean is_mounted;
    g_autofree gchar *mount_point = NULL;

    mount_point = g_dir_make_tmp("partup-XXXXXX", &fixture->error);
    g_assert_no_error(fixture->error);

    create_partition(fixture->loop_dev, &fixture->error);
    g_assert_no_error(fixture->error);

    g_assert_true(pu_mount(g_strdup_printf("%sp1", fixture->loop_dev),
                           mount_point, NULL, NULL, &fixture->error));
    g_assert_no_error(fixture->error);

    g_assert_true(pu_device_mounted(fixture->loop_dev, &is_mounted, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_true(is_mounted);

    g_assert_true(pu_umount_all(fixture->loop_dev, &fixture->error));
    g_assert_no_error(fixture->error);

    g_assert_true(pu_device_mounted(fixture->loop_dev, &is_mounted, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_false(is_mounted);

    g_assert_cmpint(g_rmdir(mount_point), ==, 0);
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

    g_test_add_func("/mount/create_mount_point", test_create_mount_point);
    g_test_add("/mount/mount", CopyFileFixture, MOUNT_SOURCE, copy_file_setup,
               test_mount, copy_file_teardown);
    g_test_add("/mount/umount", CopyFileFixture, MOUNT_SOURCE, copy_file_setup,
               test_umount, copy_file_teardown);
    g_test_add("/mount/device_mounted_false", EmptyDeviceFixture, NULL, empty_device_set_up,
               test_device_mounted_false, empty_device_tear_down);
    g_test_add("/mount/device_mounted_true", EmptyDeviceFixture, NULL, empty_device_set_up,
               test_device_mounted_true, empty_device_tear_down);
    g_test_add("/mount/umount_all", EmptyDeviceFixture, NULL, empty_device_set_up,
               test_umount_all, empty_device_tear_down);

    return g_test_run();
}
