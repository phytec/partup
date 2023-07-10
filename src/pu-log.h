/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_LOG_H
#define PARTUP_LOG_H

#include <glib.h>

void pu_log_init(void);
void pu_log_set_debug_domains(gboolean quiet,
                              gboolean debug,
                              const gchar *debug_domains);

#endif /* PARTUP_LOG_H */
