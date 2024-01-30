/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include "pu-glib-compat.h"
#include "pu-command.h"

struct _PuCommandContext {
    /* The currently used command and options for this context */
    PuCommandEntry *command;
    /* Additional arguments (null-terminated), not being commands or options */
    gchar **args;
    gsize args_len;

    /* The option context to append options to */
    GOptionContext *option_context;

    /* Command description appended to the option context */
    gchar *description;

    /* All valid command entries */
    PuCommandEntry *entries;
    gsize entries_len;

    /* If some input args were already parsed */
    gboolean parsed;
};

G_DEFINE_QUARK(pu-command-context-error-quark, pu_command_error)

PuCommandContext *
pu_command_context_new(void)
{
    PuCommandContext *context;

    context = g_new0(PuCommandContext, 1);
    context->option_context = g_option_context_new(NULL);
    context->parsed = FALSE;

    return context;
}

void
pu_command_context_free(PuCommandContext *context)
{
    g_return_if_fail(context != NULL);

    context->command = NULL;
    g_strfreev(context->args);

    g_free(context->description);
    g_free(context->entries);

    if (context->option_context)
        g_option_context_free(context->option_context);

    g_free(context);
}

static gint
pu_unichar_get_width(gunichar c)
{
    if (G_UNLIKELY(g_unichar_iszerowidth(c)))
        return 0;

    if (g_unichar_iswide(c))
        return 2;

    return 1;
}

static gint
pu_utf8_strlen(const gchar *s)
{
    gint len = 0;

    g_return_val_if_fail(s != NULL, 0);

    while (*s) {
        len += pu_unichar_get_width(g_utf8_get_char(s));
        s = g_utf8_next_char(s);
    }

    return len;
}

static gchar *
get_main_help(PuCommandContext *context)
{
    GString *string = NULL;
    GString *entry_string = NULL;
    PuCommandEntry *entry = NULL;
    g_autofree gchar *orig_help = NULL;
    g_autoptr(GRegex) regex = NULL;
    g_autoptr(GMatchInfo) match_info = NULL;
    g_autoptr(GString) match_string = NULL;
    gsize max_len = 0;

    g_return_val_if_fail(context != NULL, NULL);

    /* Get help text of option context */
    orig_help = g_option_context_get_help(context->option_context, TRUE, NULL);
    string = g_string_new(NULL);

    /* Search original help text for length of options including whitespace */
    regex = g_regex_new("  -h, --help\\s+", 0, 0, NULL);
    g_regex_match(regex, orig_help, 0, &match_info);
    if (g_match_info_matches(match_info)) {
        match_string = g_string_new(g_match_info_fetch(match_info, 0));
        max_len = match_string->len;
    } else {
        max_len = 6 + pu_utf8_strlen(context->entries[0].name);
    }

    /* Construct command help text */
    g_string_append(string, "Commands:\n");
    for (gsize i = 0; i < context->entries_len; i++) {
        entry = &context->entries[i];
        entry_string = g_string_new(NULL);

        g_string_append_printf(entry_string, "  %s", entry->name);
        g_string_append_printf(entry_string, "%*s%s\n",
                               (gint) (max_len - pu_utf8_strlen(entry_string->str)), "",
                               entry->description ? entry->description : "");
        g_string_append(string, g_string_free_and_steal(entry_string));
    }

    g_string_append_printf(string,
                           "\nExecute any command with the '-h, --help' option "
                           "to show command specific help.");

    return g_string_free_and_steal(string);
}

static const gchar *
get_command_help(PuCommandContext *context,
                 const gchar *command)
{
    PuCommandEntry *entry = NULL;

    g_return_val_if_fail(context != NULL, NULL);
    g_return_val_if_fail(command != NULL, NULL);

    /* Find command in list of valid command entries */
    for (gsize i = 0; i < context->entries_len; i++) {
        if (g_str_equal(command, context->entries[i].name)) {
            entry = &context->entries[i];
            break;
        }
    }

    if (!entry)
        return NULL;

    return entry->description;
}

static gchar *
get_help_text(PuCommandContext *context,
              const gchar *command)
{
    GString *string = NULL;

    g_return_val_if_fail(context != NULL, NULL);

    string = g_string_new(NULL);

    if (command)
        g_string_append(string, get_command_help(context, command));
    else
        g_string_append(string, get_main_help(context));

    g_string_append(string,
                    "\n\nDocumentation: <https://phytec.github.io/partup>\n"
                    "Report any issues at: <https://github.com/phytec/partup/issues>");

    return g_string_free_and_steal(string);
}

void
pu_command_context_add_entries(PuCommandContext *context,
                               const PuCommandEntry *entries,
                               const GOptionEntry *main_option_entries)
{
    gsize entries_len;

    g_return_if_fail(context != NULL);
    g_return_if_fail(entries != NULL);

    if (main_option_entries) {
        g_option_context_add_main_entries(context->option_context,
                                          main_option_entries, NULL);
    } else {
        GOptionEntry entries[] = { { NULL } };
        g_option_context_add_main_entries(context->option_context, entries, NULL);
    }

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

gchar *
pu_command_context_get_help(PuCommandContext *context,
                            const gchar *command)
{
    g_option_context_set_description(context->option_context,
                                     get_help_text(context, command));

    return g_option_context_get_help(context->option_context, TRUE, NULL);
}

gchar **
pu_command_context_get_args(PuCommandContext *context)
{
    g_return_val_if_fail(context != NULL, FALSE);
    g_return_val_if_fail(context->command != NULL, FALSE);
    g_return_val_if_fail(context->args != NULL, FALSE);

    return context->args;
}

gboolean
pu_command_context_parse(PuCommandContext *context,
                         gint *argc,
                         gchar ***argv,
                         gchar ***arg_remaining,
                         GError **error)
{
    g_return_val_if_fail(context != NULL, FALSE);
    g_return_val_if_fail(arg_remaining != NULL && *arg_remaining == NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    g_return_val_if_fail(!context->parsed, FALSE);

    if (!argc || !argv) {
        g_set_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_BAD_VALUE,
                    "No command provided");
        return FALSE;
    }

    /* Search argv for a valid command */
    for (gsize i = 0; i < context->entries_len; i++) {
        if (g_strv_contains((const gchar * const *) *argv, context->entries[i].name)) {
            context->command = &context->entries[i];
            context->command->option_entries = context->entries[i].option_entries;
            break;
        }
    }

    /* Show help text by default, if no command is provided */
    if (!context->command && !(*argv)[1]) {
        g_autofree gchar *args_old = g_strjoinv(" ", *argv);
        g_autofree gchar *args_new = g_strjoin(" ", args_old, "--help", NULL);
        g_strfreev(*argv);
        *argv = g_strsplit(args_new, " ", -1);
        (*argc)++;
    }

    if (!context->command &&
        !(g_strv_contains((const gchar * const *) *argv, "-h") ||
          g_strv_contains((const gchar * const *) *argv, "--help"))) {
        g_set_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_UNKNOWN_COMMAND,
                    "Invalid command '%s'", (*argv)[1]);
        return FALSE;
    }

    /* Set appropriate help text */
    if (context->command) {
        g_option_context_set_description(context->option_context,
                                         get_help_text(context, context->command->name));
    } else {
        g_option_context_set_description(context->option_context,
                                         get_help_text(context, NULL));
    }

    /* Add command-specific options */
    if (context->command && context->command->option_entries != NULL) {
        g_option_context_add_main_entries(context->option_context,
                                          context->command->option_entries, NULL);
    } else {
        /* Main option entries should not contain the G_OPTION_REMAINING, as it
         * would override the one provided with the command specific one. For
         * that reason, add the main usage here. */
        GOptionEntry extra_entries[] = {
            { G_OPTION_REMAINING, 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING_ARRAY,
                arg_remaining, NULL, "COMMAND" },
            { NULL }
        };
        g_option_context_add_main_entries(context->option_context,
                                          extra_entries, NULL);
    }

    /* Parse main options and command-specific ones */
    if (!g_option_context_parse(context->option_context, argc, argv, error)) {
        g_prefix_error(error, "Failed parsing options: ");
        return FALSE;
    }

    /* Retrieve remaining arguments */
    context->args_len = g_strv_length(*arg_remaining) - 1;
    context->args = g_malloc0((context->args_len + 1) * sizeof(gchar *));
    if (context->args_len > 0) {
        /* First remaining argument is the command, skip that one */
        for (gsize i = 0; i < context->args_len; i++)
            context->args[i] = g_strdup((*arg_remaining)[i + 1]);
    }

    /* Check for correct number of arguments depending on command type */
    if ((context->args_len != 0 && context->command->arg == PU_COMMAND_ARG_NONE) ||
        (context->args_len != 1 && context->command->arg == PU_COMMAND_ARG_FILENAME) ||
        (context->args_len < 2 && context->command->arg == PU_COMMAND_ARG_FILENAME_ARRAY)) {
        g_autofree gchar *excess_args = NULL;
        if (context->args)
            excess_args = g_strjoinv(" ", context->args);
        g_set_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_BAD_VALUE,
                    "Invalid number of arguments (%ld) for command '%s': '%s'",
                    context->args_len, context->command->name, excess_args);
        return FALSE;
    }

    context->parsed = TRUE;

    return TRUE;
}

gboolean
pu_command_context_parse_strv(PuCommandContext *context,
                              gchar ***args,
                              gchar ***arg_remaining,
                              GError **error)
{
    gint argc;

    g_return_val_if_fail(context != NULL, FALSE);
    g_return_val_if_fail(arg_remaining != NULL && *arg_remaining == NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    argc = args && *args ? g_strv_length(*args) : 0;

    return pu_command_context_parse(context, &argc, args, arg_remaining, error);
}

gboolean
pu_command_context_invoke(PuCommandContext *context,
                          GError **error)
{
    g_return_val_if_fail(context != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!context->command) {
        g_set_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_UNKNOWN_COMMAND,
                    "No command parsed yet");
        return FALSE;
    }

    return context->command->func(context, error);
}
