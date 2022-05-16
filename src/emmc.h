/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_EMMC_H
#define PARTUP_EMMC_H

#include "flash.h"

#define PU_TYPE_EMMC pu_emmc_get_type()

G_DECLARE_FINAL_TYPE(PuEmmc, pu_emmc, PU, EMMC, PuFlash)

PuEmmc * pu_emmc_new(const gchar *device_path,
                     PuConfigEmmc *config);

#endif /* PARTUP_EMMC_H */
