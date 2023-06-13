/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "pu-error.h"
#include "pu-mount.h"

#define MOUNT_POINT_BASENAME "partition"
#define MOUNT_POINT          PU_MOUNT_PREFIX "/" MOUNT_POINT_BASENAME 

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

    return g_test_run();
}
