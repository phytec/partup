/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2025 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_UNIT_H
#define PARTUP_UNIT_H

#include <glib.h>

gint64 pu_unit_get_factor(const gchar *unit);
gboolean pu_unit_parse_bytes(const gchar *str,
                             gint64 *bytes);

#endif /* PARTUP_UNIT_H */
