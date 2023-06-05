/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_LOG_H
#define PARTUP_LOG_H

#include <glib.h>

void pu_log_setup(gboolean quiet,
                  gboolean debug);

#endif /* PARTUP_LOG_H */
