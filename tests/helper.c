/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <glib/gstdio.h>
#include <parted/parted.h>
#include "helper.h"
#include "pu-glib-compat.h"

#define LAYOUT_CONFIG_SIMPLE "config/simple.yaml"

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

void
package_files_setup(PackageFilesFixture *fixture,
                    G_GNUC_UNUSED gconstpointer user_data)
{
    g_autofree gchar *pwd = NULL;
    g_autoptr(GFile) file1 = NULL;
    g_autoptr(GFile) file2 = NULL;
    g_autoptr(GFile) file3 = NULL;
    g_autoptr(GFile) layout_in = NULL;
    g_autoptr(GFile) layout_out = NULL;

    fixture->error = NULL;
    fixture->input_files = g_strsplit("layout.yaml file1 file2 file3", " ", -1);
    fixture->path_tmp = g_dir_make_tmp("partup-XXXXXX", &fixture->error);
    fixture->path_test = g_get_current_dir();

    g_assert_no_error(fixture->error);
    file1 = create_tmp_file(fixture->input_files[1], fixture->path_tmp,
                            1 * PED_KIBIBYTE_SIZE, &fixture->error);
    g_assert_no_error(fixture->error);
    file2 = create_tmp_file(fixture->input_files[2], fixture->path_tmp,
                            16 * PED_MEBIBYTE_SIZE, &fixture->error);
    g_assert_no_error(fixture->error);
    file3 = create_tmp_file(fixture->input_files[3], fixture->path_tmp,
                            23 * PED_KILOBYTE_SIZE, &fixture->error);
    g_assert_no_error(fixture->error);

    layout_in = g_file_new_for_path(LAYOUT_CONFIG_SIMPLE);
    layout_out = g_file_new_build_filename(fixture->path_tmp, fixture->input_files[0], NULL);
    g_file_copy(layout_in, layout_out, G_FILE_COPY_NONE, NULL, NULL, NULL, &fixture->error);
}

void
package_files_teardown(PackageFilesFixture *fixture,
                       G_GNUC_UNUSED gconstpointer user_data)
{
    for (gsize i = 0; fixture->input_files[i] != NULL; i++) {
        g_autoptr(GFile) file = g_file_new_build_filename(fixture->path_tmp,
                                                          fixture->input_files[i],
                                                          NULL);
        g_assert_true(g_file_delete(file, NULL, &fixture->error));
        g_assert_no_error(fixture->error);
    }
    g_assert_cmpint(g_rmdir(fixture->path_tmp), ==, 0);

    g_strfreev(fixture->input_files);
    g_free(fixture->path_tmp);
    g_free(fixture->path_test);
    g_clear_error(&fixture->error);
}
