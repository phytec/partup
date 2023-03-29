/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_MOUNT_H
#define PARTUP_MOUNT_H

#include <glib.h>

#define PU_MOUNT_PREFIX "/tmp/partup"

gchar * pu_create_mount_point(const gchar *name,
                              GError **error);
gboolean pu_mount(const gchar *source,
                  const gchar *mount_point,
                  const gchar *type,
                  const gchar *options,
                  GError **error);
gboolean pu_umount(const gchar *mount_point,
                   GError **error);
gboolean pu_umount_all(const gchar *device,
                       GError **error);
gboolean pu_device_mounted(const gchar *device,
                           gboolean *is_mounted,
                           GError **error);

#endif /* PARTUP_MOUNT_H */
