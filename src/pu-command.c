/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include "pu-command.h"

struct _PuCommandContext {
    PuCommandEntry *entries;
    gsize entries_len;
};

static gboolean
parse_cmd(PuCommandContext *context,
          PuCommandEntry *entry,
          const gchar *value,
          GError **error)
{
    // TODO: Assign value to entry arg_data
    return TRUE;
}

PuCommandContext *
pu_command_context_new(const gchar *args)
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
    g_return_if_fail(entry != NULL);

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
    if (argc && argv) {
        for (gint i = 1; i < *argc; i++) {
            gchar 
        }
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
