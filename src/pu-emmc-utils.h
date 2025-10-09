/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2025 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_EMMC_UTILS_H
#define PARTUP_EMMC_UTILS_H

#include <glib.h>

gboolean pu_emmc_utils_read_extcsd(const gchar *device,
                                   guint8 ext_csd[512],
                                   GError **error);
gboolean pu_emmc_utils_get_enh_area_max_size(const gchar *device,
                                             gint64 *max_size,
                                             GError **error);
gboolean pu_emmc_utils_get_enh_completed(const gchar *device,
                                         gboolean *completed,
                                         GError **error);
gboolean pu_emmc_utils_set_enh_area(const gchar *device,
                                    const gchar *enh_area,
                                    GError **error);

#endif /* PARTUP_EMMC_UTILS_H */
