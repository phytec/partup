/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include "pu-log.h"

#define PU_LOG_DOMAINS "partup partup-config partup-emmc partup-mount partup-utils"

GLogLevelFlags log_output_level;

static GLogWriterOutput
pu_log_writer(GLogLevelFlags log_level,
              const GLogField *fields,
              gsize n_fields,
              gpointer user_data)
{
    if (log_level > log_output_level)
        return G_LOG_WRITER_HANDLED;

    return g_log_writer_standard_streams(log_level, fields, n_fields, user_data);
}

void
pu_log_setup(gboolean quiet,
             gboolean debug)
{
    g_autoptr(GString) new_domains = NULL;
    const gchar *domains;

    if (quiet) {
        log_output_level = G_LOG_LEVEL_CRITICAL;
    } else if (debug) {
        domains = g_getenv("G_MESSAGES_DEBUG");

        if (domains != NULL) {
            g_string_printf(new_domains, "%s %s", domains, PU_LOG_DOMAINS);
            g_setenv("G_MESSAGES_DEBUG", new_domains->str, TRUE);
        } else {
            g_setenv("G_MESSAGES_DEBUG", PU_LOG_DOMAINS, TRUE);
        }
        log_output_level = G_LOG_LEVEL_DEBUG;
    } else {
        log_output_level = G_LOG_LEVEL_INFO;
    }

    g_log_set_writer_func(pu_log_writer, NULL, NULL);
}
