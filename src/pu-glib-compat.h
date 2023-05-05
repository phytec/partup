/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PU_GLIB_COMPAT_H
#define PU_GLIB_COMPAT_H

#include <glib.h>

#if !GLIB_CHECK_VERSION(2, 70, 0)
#define g_spawn_check_wait_status g_spawn_check_exit_status
#endif

#if !GLIB_CHECK_VERSION(2, 76, 0)
gchar * g_string_free_and_steal(GString *string) G_GNUC_WARN_UNUSED_RESULT;
#endif

#endif /* PU_GLIB_COMPAT_H */
