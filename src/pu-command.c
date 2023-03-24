/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include "pu-command.h"

struct _PuCommandContext {
    PuCommandEntry *command;
    gchar **args;

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

    context->command = NULL;
    g_strfreev(context->args);

    for (gint i = 0; context->entries != NULL; i++)
        g_free(&context->entries[i]);
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

    if (entries_len != 0) {
        memcpy(context->entries + context->entries_len, entries,
               sizeof(PuCommandEntry) * entries_len);
    }

    context->entries_len += entries_len;
}

gboolean
pu_command_context_parse(PuCommandContext *context,
                         gint *argc,
                         gchar ***argv,
                         GError **error)
{
    g_return_val_if_fail(context != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!argc || !argv) {
        g_set_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_BAD_VALUE,
                    "No command provided");
        return FALSE;
    }

    for (gsize i = 0; i < context->entries_len; i++) {
        if (g_str_equal(context->entries[i].name, (*argv)[0]))
            context->command = &context->entries[i];
    }
    if (!context->command) {
        g_set_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_UNKNOWN_COMMAND,
                    "Unknown command '%s'", (*argv)[0]);
        return FALSE;
    }

    g_free((*argv)[0]);
    for (gint i = 0; i < *argc - 1; i++)
        (*argv)[i] = (*argv)[i + 1];
    (*argc)--;
    (*argv)[*argc] = NULL;

    if ((*argc > 0 && context->command->arg == PU_COMMAND_ARG_FILENAME_ARRAY) ||
        (*argc > 1 && context->command->arg == PU_COMMAND_ARG_FILENAME)) {
        g_autofree gchar *excess_args = g_strjoinv(" ", *argv);
        g_set_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_BAD_VALUE,
                    "Excess arguments for command '%s': %s",
                    context->command->name, excess_args);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_command_context_parse_strv(PuCommandContext *context,
                              gchar ***args,
                              GError **error)
{
    gboolean res;
    gint argc;

    g_return_val_if_fail(context != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    argc = args && *args ? g_strv_length(*args) : 0;
    res = pu_command_context_parse(context, &argc, args, error);

    return res;
}

gboolean
pu_command_context_invoke(PuCommandContext *context,
                          GError **error)
{
    g_return_val_if_fail(context != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    return context->command->func(context->args, error);
}
