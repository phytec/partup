/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_TEST_HELPER_H
#define PARTUP_TEST_HELPER_H

#include <glib.h>

typedef struct {
    GError *error;
    GFile *part;
    gchar *path;
} EmptyFileFixture;

GFile * create_tmp_file(const gchar *filename,
                        const gchar *pwd,
                        gsize size,
                        GError **error);
void empty_file_set_up(EmptyFileFixture *fixture,
                       G_GNUC_UNUSED gconstpointer user_data);
void empty_file_tear_down(EmptyFileFixture *fixture,
                          G_GNUC_UNUSED gconstpointer user_data);

#endif /* PARTUP_TEST_HELPER_H */
