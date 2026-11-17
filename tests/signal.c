/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <signal.h>
#include "pu-error.h"
#include "pu-signal.h"

static void
test_signal_check_not_cancelled(void)
{
    g_autoptr(GError) error = NULL;

    pu_signal_init();
    g_assert_false(pu_signal_is_cancelled());
    g_assert_true(pu_signal_check(&error));
    g_assert_no_error(error);
    pu_signal_reset();
}

static void
test_signal_check_sigint(void)
{
    g_autoptr(GError) error = NULL;

    pu_signal_init();
    raise(SIGINT);
    g_assert_true(pu_signal_is_cancelled());
    g_assert_false(pu_signal_check(&error));
    g_assert_error(error, PU_ERROR, PU_ERROR_CANCELLED);
    pu_signal_reset();
}

static void
test_signal_check_sigterm(void)
{
    g_autoptr(GError) error = NULL;

    pu_signal_init();
    raise(SIGTERM);
    g_assert_false(pu_signal_check(&error));
    g_assert_error(error, PU_ERROR, PU_ERROR_CANCELLED);
    pu_signal_reset();
}

int
main(int argc,
     char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/signal/check-not-cancelled", test_signal_check_not_cancelled);
    g_test_add_func("/signal/check-sigint", test_signal_check_sigint);
    g_test_add_func("/signal/check-sigterm", test_signal_check_sigterm);

    return g_test_run();
}
