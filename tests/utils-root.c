/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <blkid.h>
#include "helper.h"
#include "pu-glib-compat.h"
#include "pu-utils.h"
#include "pu-error.h"

#define PARTUUID "428957a5-839d-45ae-adaf-4108c98a087b"

static void
test_is_drive(EmptyDeviceFixture *fixture,
              G_GNUC_UNUSED gconstpointer user_data)
{
    g_assert_true(pu_is_drive(fixture->loop_dev));
}

static void
test_wait_for_partitions(void)
{
    g_autoptr(GError) error = NULL;
    g_assert_true(pu_wait_for_partitions(&error));
    g_assert_no_error(error);
}

static void
test_partition_set_partuuid(EmptyDeviceFixture *fixture,
                            G_GNUC_UNUSED gconstpointer user_data)
{
    blkid_probe pr;
    blkid_partlist ls;
    blkid_partition par;
    const gchar *partuuid;
    gint wait_status;
    g_autofree gchar *cmd = g_strdup_printf("sh scripts/create-partition %s",
                                            fixture->loop_dev);
    g_assert_true(g_spawn_command_line_sync(cmd, NULL, NULL,
                                            &wait_status, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_true(g_spawn_check_wait_status(wait_status, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_true(pu_partition_set_partuuid(fixture->loop_dev, 1,
                                            PARTUUID, &fixture->error));
    g_assert_no_error(fixture->error);

    pr = blkid_new_probe_from_filename(fixture->loop_dev);
    g_assert_nonnull(pr);
    ls = blkid_probe_get_partitions(pr);
    g_assert_nonnull(ls);
    par = blkid_partlist_get_partition(ls, 0);
    g_assert_nonnull(par);
    partuuid = blkid_partition_get_uuid(par);
    g_assert_cmpstr(partuuid, ==, PARTUUID);
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

    g_test_add("/utils/is_drive", EmptyDeviceFixture, NULL, empty_device_set_up,
               test_is_drive, empty_device_tear_down);
    g_test_add_func("/utils/wait_for_partitions", test_wait_for_partitions);
    g_test_add("/utils/part_set_partuuid", EmptyDeviceFixture, NULL, empty_device_set_up,
               test_partition_set_partuuid, empty_device_tear_down);

    return g_test_run();
}
