/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <stdio.h>
#include "pu-log.h"

#define PU_LOG_DOMAINS "partup partup-config partup-emmc partup-mount partup-package partup-utils"

GLogLevelFlags log_output_level;

static void
append_log_time(GString *log_str)
{
    g_autoptr(GDateTime) datetime = NULL;

    datetime = g_date_time_new_now_local();
    g_string_append_printf(log_str, "%s", g_date_time_format(datetime, "%H:%M:%S.%f"));
}

static void
append_log_level(GString *log_str,
                 GLogLevelFlags log_level,
                 gboolean color)
{
    if (color) {
        switch (log_level) {
        case G_LOG_LEVEL_ERROR:
            g_string_append(log_str, "\033[1;35mERROR\033[0m");
            break;
        case G_LOG_LEVEL_CRITICAL:
            g_string_append(log_str, "\033[1;31mCRITICAL\033[0m");
            break;
        case G_LOG_LEVEL_WARNING:
            g_string_append(log_str, "\033[1;33mWARNING\033[0m");
            break;
        case G_LOG_LEVEL_MESSAGE:
            g_string_append(log_str, "\033[1;34mMESSAGE\033[0m");
            break;
        case G_LOG_LEVEL_INFO:
            g_string_append(log_str, "\033[1;36mINFO\033[0m");
            break;
        case G_LOG_LEVEL_DEBUG:
            g_string_append(log_str, "\033[1;32mDEBUG\033[0m");
            break;
        default:
            g_string_append(log_str, "\033[1;37mUNKNOWN\033[0m");
        }
    } else {
        switch (log_level) {
        case G_LOG_LEVEL_ERROR:
            g_string_append(log_str, "ERROR");
            break;
        case G_LOG_LEVEL_CRITICAL:
            g_string_append(log_str, "CRITICAL");
            break;
        case G_LOG_LEVEL_WARNING:
            g_string_append(log_str, "WARNING");
            break;
        case G_LOG_LEVEL_MESSAGE:
            g_string_append(log_str, "MESSAGE");
            break;
        case G_LOG_LEVEL_INFO:
            g_string_append(log_str, "INFO");
            break;
        case G_LOG_LEVEL_DEBUG:
            g_string_append(log_str, "DEBUG");
            break;
        default:
            g_string_append(log_str, "UNKNOWN");
        }
    }
}

static void
append_log_domain(GString *log_str,
                  const gchar *log_domain,
                  gboolean color)
{
    g_return_if_fail(log_domain && *log_domain);

    if (color)
        g_string_append(log_str, "\033[0;37m");

    g_string_append_printf(log_str, "%s", log_domain);

    if (color)
        g_string_append(log_str, "\033[0m");
}

static GLogWriterOutput
pu_log_writer_formatted(GLogLevelFlags log_level,
                        const gchar *log_domain,
                        const gchar *log_message,
                        G_GNUC_UNUSED const GLogField *fields,
                        G_GNUC_UNUSED gsize n_fields)
{
    g_autoptr(GString) log_str = NULL;
    gboolean use_color;
    FILE *stream = stdout;

    log_str = g_string_new(NULL);
    use_color = g_log_writer_supports_color(fileno(stream));

    if (log_output_level > G_LOG_LEVEL_MESSAGE || log_level < G_LOG_LEVEL_MESSAGE) {
        append_log_time(log_str);
        g_string_append_c(log_str, ' ');
        append_log_level(log_str, log_level, use_color);
        g_string_append_c(log_str, ' ');
        append_log_domain(log_str, log_domain, use_color);
        g_string_append(log_str, ": ");
    }

    g_string_append_printf(log_str, "%s", log_message);

    fprintf(stream, "%s\n", log_str->str);
    fflush(stream);

    if (log_level & G_LOG_FLAG_FATAL)
        g_abort();

    return G_LOG_WRITER_HANDLED;
}

static GLogWriterOutput
pu_log_writer(GLogLevelFlags log_level,
              const GLogField *fields,
              gsize n_fields,
              G_GNUC_UNUSED gpointer user_data)
{
    const gchar *log_domain = NULL;
    const gchar *log_message = NULL;

    for (gsize i = 0; (!log_domain || !log_message) && i < n_fields; i++) {
        if (g_strcmp0(fields[i].key, "GLIB_DOMAIN") == 0)
            log_domain = fields[i].value;
        else if (g_strcmp0(fields[i].key, "MESSAGE") == 0)
            log_message = fields[i].value;
    }

    if (!log_domain)
        log_domain = "(NULL domain)";
    if (!log_message)
        log_message = "(NULL message)";

    if (log_level > log_output_level || g_log_writer_default_would_drop(log_level, log_domain))
        return G_LOG_WRITER_HANDLED;

    return pu_log_writer_formatted(log_level, log_domain, log_message, fields, n_fields);
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
        log_output_level = G_LOG_LEVEL_MESSAGE;
    }

    g_log_set_writer_func(pu_log_writer, NULL, NULL);
}
