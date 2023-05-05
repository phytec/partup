/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include "pu-glib-compat.h"

#if !GLIB_CHECK_VERSION(2, 76, 0)
gchar *
g_string_free_and_steal(GString *string)
{
    return g_string_free(string, FALSE);
}
#endif
