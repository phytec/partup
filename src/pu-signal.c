/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <signal.h>
#include <string.h>
#include "pu-error.h"
#include "pu-signal.h"

static volatile sig_atomic_t cancelled = 0;

static void
signal_handler(G_GNUC_UNUSED int signum)
{
    /* SA_RESETHAND restores the default action, so that a second signal
     * terminates the process immediately. */
    cancelled = 1;
}

void
pu_signal_init(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    /* No SA_RESTART, so that blocking calls return with EINTR */
    sa.sa_flags = SA_RESETHAND;

    cancelled = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

void
pu_signal_reset(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);

    cancelled = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

gboolean
pu_signal_is_cancelled(void)
{
    return cancelled != 0;
}

gboolean
pu_signal_check(GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!cancelled)
        return TRUE;

    g_set_error(error, PU_ERROR, PU_ERROR_CANCELLED, "Aborted by user");
    return FALSE;
}
