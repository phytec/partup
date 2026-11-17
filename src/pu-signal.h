/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_SIGNAL_H
#define PARTUP_SIGNAL_H

#include <glib.h>

void pu_signal_init(void);
void pu_signal_reset(void);
gboolean pu_signal_is_cancelled(void);
gboolean pu_signal_check(GError **error);

#endif /* PARTUP_SIGNAL_H */
