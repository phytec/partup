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
    // TODO: Free individual entries and their content
    for (gint i = 0; context->entries != NULL; i++) {
        g_free(context->entries[i].name);
        g_free(context->entries[i].description);
    }
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
    g_debug("entries_len=%ld context->entries_len=%ld", entries_len, context->entries_len);

    context->entries = g_renew(PuCommandEntry, context->entries,
                               context->entries_len + entries_len);

    if (entries_len != 0) {
        memcpy(context->entries + context->entries_len, entries,
               sizeof(PuCommandEntry) * entries_len);
    }

    context->entries_len += entries_len;

    for (gsize i = 0; i < context->entries_len; i++) {
        g_debug("entry name=%s", context->entries[i].name);
    }
}

gboolean
pu_command_context_parse(PuCommandContext *context,
                         gint *argc,
                         gchar ***argv,
                         GError **error)
{
    g_debug(G_STRFUNC);

    g_return_val_if_fail(context != NULL, FALSE);

    if (!argc || !argv) {
        g_set_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_BAD_VALUE,
                    "No command provided");
        return FALSE;
    }

    // Get first value being the command
    g_debug("command: %s", (*argv)[0]);
    // Check if valid command, search in entries
    for (gsize i = 0; i < context->entries_len; i++) {
        if (g_str_equal(context->entries[i].name, (*argv)[0])) {
            context->command = &context->entries[i];
            g_debug("found valid command '%s'", context->command->name);
        }
    }
    if (!context->command) {
        g_set_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_UNKNOWN_COMMAND,
                    "Unknown command '%s'", (*argv)[0]);
        return FALSE;
    }
    // Consume first arg
    g_debug("unmod args=%s, argc=%d", g_strjoinv(" ", *argv), *argc);
    g_debug("Consume first arg of %s", g_strjoinv("", *argv));
    g_free((*argv)[0]);
    g_debug("A");
    for (gint i = 0; i < *argc - 1; i++) {
        g_debug("pre %d: %s", i, (*argv)[i]);
        (*argv)[i] = (*argv)[i + 1];
        //(*argv)[i + 1] = NULL;
        g_debug("post %d: %s", i, (*argv)[i]);
    }
    (*argc)--;
    (*argv)[*argc] = NULL;
    g_debug("left args=%s", g_strjoinv(" ", *argv));

    // Depending on command type, save additional argv to value
    g_debug("Save additional args to value");
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

    argc = args && *args ? g_strv_length(*args) : 0;
    res = pu_command_context_parse(context, &argc, args, error);

    return res;
}

gboolean
pu_command_context_invoke(PuCommandContext *context,
                          GError **error)
{
    g_debug("Invoking command '%s' with args='%s'",
            context->command->name, context->args);
    return context->command->func(context->args, error);
}
