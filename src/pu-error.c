/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2021 PHYTEC Messtechnik GmbH
 */

#include "pu-error.h"

GQuark pu_error_quark(void)
{
    return g_quark_from_static_string("pu_error_quark");
}
