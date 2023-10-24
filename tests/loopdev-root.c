/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "pu-error.h"
#include "pu-loopdev.h"

#define LOOPDEV_SIZE 1 * 1024 * 1024

static void
test_loopdev(void)
{
    g_autoptr(GError) error = NULL;
    PuLoopdev *loopdev;

    loopdev = pu_loopdev_new(LOOPDEV_SIZE, &error);
    g_assert_no_error(error);
    g_assert_nonnull(loopdev);
    g_assert_true(g_file_test(g_file_get_path(loopdev->file), G_FILE_TEST_IS_REGULAR));

    g_assert_true(pu_loopdev_attach(loopdev, &error));
    g_assert_no_error(error);

    g_assert_true(pu_loopdev_detach(loopdev, &error));
    g_assert_no_error(error);

    pu_loopdev_free(loopdev);
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

    g_test_add_func("/loopdev/loopdev", test_loopdev);

    return g_test_run();
}
