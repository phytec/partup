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
    const gchar *log_domain = NULL;

    for (gsize i = 0; i < n_fields; i++) {
        if (g_strcmp0(fields[i].key, "GLIB_DOMAIN") == 0) {
            log_domain = fields[i].value;
            break;
        }
    }

    if (log_level > log_output_level || g_log_writer_default_would_drop(log_level, log_domain))
        return G_LOG_WRITER_HANDLED;

    return g_log_writer_standard_streams(log_level, fields, n_fields, user_data);
}

void
pu_log_setup(gboolean quiet,
             gboolean debug,
             const gchar *debug_domains)
{
    g_autoptr(GString) new_domains = NULL;
    const gchar *domains;
    gchar **debug_arr;

    if (quiet) {
        log_output_level = G_LOG_LEVEL_CRITICAL;
    } else if (debug && !debug_domains) {
        domains = g_getenv("G_MESSAGES_DEBUG");

        if (domains != NULL) {
            g_string_printf(new_domains, "%s %s", domains, PU_LOG_DOMAINS);
            g_setenv("G_MESSAGES_DEBUG", new_domains->str, TRUE);
        } else {
            g_setenv("G_MESSAGES_DEBUG", PU_LOG_DOMAINS, TRUE);
        }
        log_output_level = G_LOG_LEVEL_DEBUG;
    } else if (debug_domains) {
        new_domains = g_string_new(g_getenv("G_MESSAGES_DEBUG"));
        debug_arr = g_strsplit(debug_domains, ",", 0);
        for (int i = 0; debug_arr[i] != NULL; i++) {
            g_string_append(new_domains,
                            g_strdup_printf("partup-%s", debug_arr[i]));
        }
        g_strfreev(debug_arr);
        g_setenv("G_MESSAGES_DEBUG", new_domains->str, TRUE);
        log_output_level = G_LOG_LEVEL_DEBUG;
    } else {
        log_output_level = G_LOG_LEVEL_INFO;
    }

    g_log_set_writer_func(pu_log_writer, NULL, NULL);
}
