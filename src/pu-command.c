/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include "pu-command.h"

struct _PuCommandContext {
    PuCommandEntry *command;
    gchar **args;

    GOptionContext *option_context;

    PuCommandEntry *entries;
    gsize entries_len;
};

G_DEFINE_QUARK(pu-command-context-error-quark, pu_command_error)

PuCommandContext *
pu_command_context_new(GOptionContext *option_context)
{
    PuCommandContext *context;

    context = g_new0(PuCommandContext, 1);
    context->option_context = option_context;

    return context;
}

void
pu_command_context_free(PuCommandContext *context)
{
    g_return_if_fail(context != NULL);

    context->command = NULL;
    g_strfreev(context->args);

    /*g_debug("context->entries_len=%ld", context->entries_len);
    for (gsize i = 0; i < context->entries_len; i++) {
        g_debug("%s: freeing %ld '%s'", G_STRFUNC, i, context->entries[i].name);
        //g_free(context->entries[i].name);
        //g_free(context->entries[i].description);
    }*/
    g_free(context->entries);
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

    g_debug("entries_len=%ld context->entries_len=%ld",
            entries_len, context->entries_len);
    context->entries = g_renew(PuCommandEntry, context->entries,
                               context->entries_len + entries_len);

    if (entries_len != 0) {
        memcpy(context->entries + context->entries_len, entries,
               sizeof(PuCommandEntry) * entries_len);
    }

    context->entries_len += entries_len;
    g_debug("entries_len=%ld context->entries_len=%ld",
            entries_len, context->entries_len);
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

    //(*argv)[0] = NULL;
    for (gint i = 0; i < *argc - 1; i++) {
        g_debug("Freeing %d with old content '%s'", i, (*argv)[i]);
        g_free((*argv)[i]);
        g_debug("Assigning %d with new content '%s'", i, (*argv)[i + 1]);
        (*argv)[i] = g_strdup((*argv)[i + 1]);
    }
    (*argc)--;
    g_debug("Freeing %d with old content '%s'", *argc, (*argv)[*argc]);
    g_free((*argv)[*argc]);
    (*argv)[*argc] = NULL;

    g_debug("*argc=%d ***argv=%s", *argc, g_strjoinv(" ", *argv));
    if ((*argc != 0 && context->command->arg == PU_COMMAND_ARG_NONE) ||
        (*argc != 1 && context->command->arg == PU_COMMAND_ARG_FILENAME)) {
        g_autofree gchar *excess_args = g_strjoinv(" ", *argv + 1);
        g_set_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_BAD_VALUE,
                    "Excess arguments for command '%s': %s",
                    context->command->name, excess_args);
        return FALSE;
    }

    context->args = g_strdupv(*argv);

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

    return context->command->func(context->args, context->option_context, error);
}
