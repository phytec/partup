/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <parted/parted.h>
#include "helper.h"
#include "pu-glib-compat.h"

GFile *
create_tmp_file(const gchar *filename,
                const gchar *pwd,
                gsize size,
                GError **error)
{
    GFile *file;
    g_autoptr(GFileIOStream) stream = NULL;
    g_autofree gchar *buffer = NULL;

    file = g_file_new_build_filename(pwd, filename, NULL);
    g_assert_nonnull(file);
    buffer = g_new0(gchar, size);
    g_assert_true(g_file_replace_contents(file, buffer, size, NULL, FALSE, 0, NULL, NULL, error));
    g_assert_no_error(*error);

    return g_steal_pointer(&file);
}

void
empty_file_set_up(EmptyFileFixture *fixture,
                  G_GNUC_UNUSED gconstpointer user_data)
{
    fixture->error = NULL;
    fixture->path = g_dir_make_tmp("partup-XXXXXX", &fixture->error);
    g_assert_no_error(fixture->error);
    fixture->part = create_tmp_file("part", fixture->path, 100 * PED_MEBIBYTE_SIZE,
                                    &fixture->error);
    g_assert_no_error(fixture->error);
}

void
empty_file_tear_down(EmptyFileFixture *fixture,
                     G_GNUC_UNUSED gconstpointer user_data)
{
    g_assert_true(g_file_delete(fixture->part, NULL, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_cmpint(g_rmdir(fixture->path), ==, 0);
}

void
empty_device_set_up(EmptyDeviceFixture *fixture,
                    G_GNUC_UNUSED gconstpointer user_data)
{
    gint wait_status;
    g_autofree gchar *cmd;

    fixture->error = NULL;
    fixture->path = g_dir_make_tmp("partup-XXXXXX", &fixture->error);
    g_assert_no_error(fixture->error);
    fixture->device = create_tmp_file("device", fixture->path, 100 * PED_MEBIBYTE_SIZE,
                                      &fixture->error);
    g_assert_no_error(fixture->error);
    cmd = g_strdup_printf("losetup -f --show -P %s", g_file_get_path(fixture->device));
    g_assert_true(g_spawn_command_line_sync(cmd, &fixture->loop_dev, NULL,
                                            &wait_status, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_true(g_spawn_check_wait_status(wait_status, &fixture->error));
    g_assert_no_error(fixture->error);
    fixture->loop_dev = g_strstrip(fixture->loop_dev);
}

void
empty_device_tear_down(EmptyDeviceFixture *fixture,
                       G_GNUC_UNUSED gconstpointer user_data)
{
    gint wait_status;
    g_autofree gchar *cmd = g_strdup_printf("losetup -d %s", fixture->loop_dev);

    g_assert_true(g_spawn_command_line_sync(cmd, NULL, NULL, &wait_status,
                                            &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_true(g_spawn_check_wait_status(wait_status, &fixture->error));
    g_assert_no_error(fixture->error);

    g_assert_true(g_file_delete(fixture->device, NULL, &fixture->error));
    g_assert_no_error(fixture->error);
    g_assert_cmpint(g_rmdir(fixture->path), ==, 0);
}
