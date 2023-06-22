/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_TEST_HELPER_H
#define PARTUP_TEST_HELPER_H

#include <glib.h>
#include <gio/gio.h>

typedef struct {
    GError *error;
    GFile *file;
    gchar *path;
} EmptyFileFixture;

typedef struct {
    GError *error;
    GFile *device;
    gchar *path;
    gchar *loop_dev;
} EmptyDeviceFixture;

typedef struct {
    gchar **input_files;
    gchar *path_tmp;
    gchar *path_test;
    GError *error;
} PackageFilesFixture;

GFile * create_tmp_file(const gchar *filename,
                        const gchar *pwd,
                        gsize size,
                        GError **error);
void empty_file_set_up(EmptyFileFixture *fixture,
                       gconstpointer filename);
void empty_file_tear_down(EmptyFileFixture *fixture,
                          gconstpointer user_data);
void empty_device_set_up(EmptyDeviceFixture *fixture,
                         gconstpointer user_data);
void empty_device_tear_down(EmptyDeviceFixture *fixture,
                            gconstpointer user_data);
void package_files_setup(PackageFilesFixture *fixture,
                         gconstpointer user_data);
void package_files_teardown(PackageFilesFixture *fixture,
                            gconstpointer user_data);

#endif /* PARTUP_TEST_HELPER_H */
