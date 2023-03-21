/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include "pu-command.h"

struct _PuCommandContext {
    PuCommandEntry *command;
    union {
        gchar *str;
        gchar **str_array;
    } args;

    PuCommandEntry *entries;
    gsize entries_len;
};

G_DEFINE_QUARK(pu-command-context-error-quark, pu_command_error)

PuCommandContext *
pu_command_context_new(void)
{
    PuCommandContext *context;

    context = g_new0(PuCommandContext, 1);

    return context;
}

void
pu_command_context_free(PuCommandContext *context)
{
    g_return_if_fail(context != NULL);

    // TODO: Free individual entries and their content
}

void
pu_command_context_add_entries(PuCommandContext *context,
                               const PuCommandEntry *entries)
{
    gsize entries_len;

    g_return_if_fail(context != NULL);
    g_return_if_fail(entries != NULL);

    for (entries_len = 0; entries[entries_len].name != NULL; entries_len++);
    g_return_if_fail(entries_len <= G_MAXSIZE - context->entries_len);

    context->entries = g_renew(PuCommandEntry, context->entries,
                               context->entries_len + entries_len);

    if (entries_len != 0)
        memcpy(context->entries + context->entries_len, entry, sizeof(PuCommandEntry));

    context->entries_len++;
}

gboolean
pu_command_context_parse(PuCommandContext *context,
                         gint *argc,
                         gchar ***argv,
                         GError **error)
{
    if (!argc || !argv) {
        g_set_error(PU_COMMAND_ERROR, PU_COMMAND_ERROR_BAD_VALUE,
                    "No command provided");
        return FALSE;
    }

    // Get first value being the command
    g_debug("command: %s", (**argv)[0]);
    // Check if valid command, search in entries
    for (gint i = 0; i < context->entries_len; i++) {
        if (g_str_equal(context->entries[i]->name, (**argv)[0]))
            context->command = context->entries[i];
    }
    if (!context->command) {
        g_set_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_UNKNOWN_COMMAND,
                    "Unknown command '%s'", (**argv)[0]);
        return FALSE;
    }
    // Consume first arg
    g_free((**argv)[0]);
    for (gint i = 0; i < *argc; i++) {
        (*argv)[i] = (*argv[i + 1]);
    }
    (*argc)--;

    // Depending on command type, save additional argv to value
    switch (context->command->arg) {
    case PU_COMMAND_ARG_NONE:
        if (*argc > 0) {
            g_autofree gchar *excess_args = g_strjoinv(" ", *argv);
            g_set_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_BAD_VALUE,
                        "Excess arguments for command '%s': %s",
                        context->command->name, excess_args);
            return FALSE;
        }
        break;
    case PU_COMMAND_ARG_FILENAME:
        if (*argc > 1) {
            g_autofree gchar *excess_args = g_strjoinv(" ", *argv);
            g_set_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_BAD_VALUE,
                        "Excess arguments for command '%s': %s",
                        context->command->name, excess_args);
            return FALSE;
        }
        context->args->str = g_strdup((**argv)[0]);
        break;
    case PU_COMMAND_ARG_FILENAME_ARRAY:
        context->args->str_array = g_strdupv(**argv);
        break;
    }

    return TRUE;
}

gboolean
pu_command_context_parse_strv(PuCommandContext *context,
                              gchar ***arguments,
                              GError **error)
{
    gboolean res;
    gint argc;

    g_return_val_if_fail(context != NULL, FALSE);

    argc = arguments && *arguments ? g_strv_length(*arguments) : 0;
    res = pu_command_context_parse(context, &argc, arguments, error);

    return res;
}
