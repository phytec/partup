/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_TEST_HELPER_H
#define PARTUP_TEST_HELPER_H

#include <glib.h>

GFile * create_tmp_file(const gchar *filename,
                        const gchar *pwd,
                        gsize size,
                        GError **error);

#endif /* PARTUP_TEST_HELPER_H */
