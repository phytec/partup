/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_MOUNT_H
#define PARTUP_MOUNT_H

#include <glib.h>

#define PU_MOUNT_PREFIX "/mnt/partup"

gchar * pu_create_mount_point(const gchar *name,
                              GError **error);
gboolean pu_mount(const gchar *source,
                  const gchar *mount_point,
                  GError **error);
gboolean pu_umount(const gchar *mount_point,
                   GError **error);
gboolean pu_device_mounted(const gchar *device);

#endif /* PARTUP_MOUNT_H */
