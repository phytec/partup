/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_ERROR_H
#define PARTUP_ERROR_H

#include <glib.h>

#define PU_ERROR pu_error_quark()

typedef enum {
    /* Generic error */
    PU_ERROR_FAILED,

    /* Flash setup errors */
    PU_ERROR_FLASH_INIT,
    PU_ERROR_FLASH_LAYOUT,
    PU_ERROR_FLASH_DATA,

    /* eMMC errors */
    PU_ERROR_EMMC_PARSE,

    /* Config parsing errors */
    PU_ERROR_CONFIG_INIT_FAILED,
    PU_ERROR_CONFIG_INVALID_ROOT,
    PU_ERROR_CONFIG_PARSING_FAILED,

    /* Checksum errors */
    PU_ERROR_CHECKSUM,

    /* Utility errors */
    PU_ERROR_UNKNOWN_FSTYPE
} PuErrorEnum;

GQuark pu_error_quark(void);

#endif /* PARTUP_ERROR_H */
