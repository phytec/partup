/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_PACKAGE_H
#define PARTUP_PACKAGE_H

#include <glib.h>
#include "pu-mount.h"

#define PU_PACKAGE_ERROR (pu_package_error_quark())

GQuark pu_package_error_quark(void);

typedef enum {
    PU_PACKAGE_ERROR_CREATION_FAILED,
    PU_PACKAGE_ERROR_EXISTS,
    PU_PACKAGE_ERROR_ITER_FAILED,
    PU_PACKAGE_ERROR_MISSING_LAYOUT,
    PU_PACKAGE_ERROR_MULTIPLE_LAYOUT,
    PU_PACKAGE_ERROR_NOT_FOUND,
    PU_PACKAGE_ERROR_FAILED
} PuPackageError;

#define PU_PACKAGE_BASENAME        "package"
#define PU_PACKAGE_PREFIX          PU_MOUNT_PREFIX "/" PU_PACKAGE_BASENAME

gchar * pu_package_get_layout_file(const gchar *pwd,
                                   GError **error);
gboolean pu_package_create(gchar **files,
                           const gchar *output,
                           gboolean force_overwrite,
                           GError **error);
gboolean pu_package_show_contents(const gchar *package,
                                  gboolean print_size,
                                  GError **error);
gboolean pu_package_mount(const gchar *package,
                          gchar **mountpoint,
                          gchar **layout_file,
                          GError **error);
gboolean pu_package_umount(const gchar *mountpoint,
                           GError **error);

#endif /* PARTUP_PACKAGE_H */
