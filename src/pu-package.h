/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_PACKAGE_H
#define PARTUP_PACKAGE_H

gboolean pu_package_create(const gchar *input_dir,
                           const gchar *output_dir,
                           GError **error);

#endif /* PARTUP_PACKAGE_H */
