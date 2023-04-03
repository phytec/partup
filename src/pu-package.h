/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_PACKAGE_H
#define PARTUP_PACKAGE_H

#include <glib.h>
#include "pu-mount.h"

#define PU_PACKAGE_BASENAME    "package"
#define PU_PACKAGE_PREFIX      PU_MOUNT_PREFIX "/" PU_PACKAGE_BASENAME
#define PU_PACKAGE_LAYOUT_FILE PU_PACKAGE_PREFIX "/layout.yaml"

gboolean pu_package_create(GPtrArray *files,
                           const gchar *output,
                           GError **error);
gboolean pu_package_list_contents(const gchar *package,
                                  GError **error);

#endif /* PARTUP_PACKAGE_H */
