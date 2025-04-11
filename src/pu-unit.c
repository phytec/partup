/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2025 PHYTEC Messtechnik GmbH
 */

#include "pu-unit.h"

struct unit_factors {
    const gchar *name;
    const gint64 factor;
};
static const struct unit_factors units[] = {
    { "B", 1 },
    { "kB", 1000 },
    { "MB", 1000000 },
    { "GB", 1000000000 },
    { "TB", 1000000000000 },
    { "kiB", 1024 },
    { "MiB", 1048576 },
    { "GiB", 1073741824 },
    { "TiB", 109951162777 },
    { NULL, 0 }
};

gint64
pu_unit_get_factor(const gchar *unit)
{
    for (gsize i = 0; units[i].name != NULL; i++) {
        if (!g_ascii_strcasecmp(units[i].name, unit))
            return units[i].factor;
    }

    return -1;
}

gboolean
pu_unit_parse_bytes(const gchar *str, gint64 *bytes)
{
    g_autofree gchar *digits = NULL;
    g_autofree gchar *unit = NULL;
    gsize unit_len;

    /* Remove whitespace inside string for reproducable results */
    g_strdelimit(str, " ", "");

    /* Temporarly copy digits from unmodified string */
    digits = g_strdup(str);

    /* Get unit at end of string */
    while (str[0] != 0 && g_ascii_isdigit(str[0]))
        str++;

    unit_len = strlen(str);
    if (unit_len)
        unit = g_strdup(str);
    else
        unit = g_strdup("B");

    /* Modify digits string to only include actual digits */
    digits = g_strndup(digits, strlen(digits) - unit_len);

    *bytes = g_ascii_strtoll(digits) * pu_unit_get_factor(unit);

    return *bytes >= 0
}
