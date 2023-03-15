/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_PACKAGE_H
#define PARTUP_PACKAGE_H

#include <glib.h>

#define PU_PACKAGE_PREFIX PU_MOUNT_PREFIX "/package"

gboolean pu_package_create(GPtrArray *files,
                           const gchar *output,
                           GError **error);
gboolean pu_package_mount(const gchar *package_path,
                          GError **error);

#endif /* PARTUP_PACKAGE_H */
