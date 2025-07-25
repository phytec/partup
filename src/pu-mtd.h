/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2025 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_MTD_H
#define PARTUP_MTD_H

#include "pu-config.h"
#include "pu-flash.h"

#define PU_TYPE_MTD pu_mtd_get_type()

G_DECLARE_FINAL_TYPE(PuMtd, pu_mtd, PU, MTD, PuFlash)

PuMtd * pu_mtd_new(const gchar *device_path,
                   PuConfig *config,
                   const gchar *prefix,
                   gboolean skip_checksums,
                   GError **error);

#endif /* PARTUP_MTD_H */
