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

#endif /* PU_GLIB_COMPAT_H */
