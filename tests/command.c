/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "pu-command.h"
#include "pu-error.h"

#define gen_argv(args) \
    g_strsplit(g_strdup_printf("%s %s", g_get_prgname(), args), " ", -1)

static gboolean
cmd_empty(PuCommandContext *context,
          GError **error)
{
    g_assert(context);
    g_assert_no_error(*error);

    return TRUE;
}

static gboolean
cmd_basic_test(PuCommandContext *context,
               GError **error)
{
    g_assert(context);
    g_assert_no_error(*error);

    g_assert_cmpstrv(NULL, pu_command_context_get_args(context)[0]);

    return TRUE;
}

static void
command_basic(void)
{
    g_autoptr(PuCommandContext) context = NULL;
    g_autoptr(GError) error = NULL;

    gchar **args = gen_argv("test");
    PuCommandEntry entries[] =
        { { "test", PU_COMMAND_ARG_NONE, cmd_basic_test,
            "Test command description", NULL },
          PU_COMMAND_ENTRY_NULL };

    context = pu_command_context_new();
    g_assert_nonnull(context);
    pu_command_context_add_entries(context, entries, NULL);
    pu_command_context_parse_strv(context, &args, &error);
    g_assert_no_error(error);

    g_assert(pu_command_context_invoke(context, &error));
    g_assert_no_error(error);

    g_strfreev(args);
}

static void
command_help(void)
{
    g_autoptr(PuCommandContext) context = NULL;
    g_autoptr(GError) error = NULL;
    g_autofree gchar *str = NULL;

    PuCommandEntry entries[] =
        { { "foo", PU_COMMAND_ARG_NONE, cmd_empty,
            "foo command description", NULL },
          { "bar", PU_COMMAND_ARG_NONE, cmd_empty,
            "bar command description", NULL },
          PU_COMMAND_ENTRY_NULL };

    context = pu_command_context_new();
    g_assert_nonnull(context);
    pu_command_context_add_entries(context, entries, NULL);

    str = pu_command_context_get_help(context, NULL);
    g_assert(strstr(str, "Commands") != NULL);
    g_assert(strstr(str, entries[0].name) != NULL);
    g_assert(strstr(str, entries[0].description) != NULL);
    g_assert(strstr(str, entries[1].name) != NULL);
    g_assert(strstr(str, entries[1].description) != NULL);

    g_free(str);
    str = pu_command_context_get_help(context, entries[0].name);
    g_assert(strstr(str, entries[0].name) != NULL);
    g_assert(strstr(str, "command description") != NULL);
    g_assert(strstr(str, entries[0].description) != NULL);

    g_free(str);
    str = pu_command_context_get_help(context, entries[1].name);
    g_assert(strstr(str, entries[1].name) != NULL);
    g_assert(strstr(str, "command description") != NULL);
    g_assert(strstr(str, entries[1].description) != NULL);
}

#define cmd_error_out() \
{ \
    g_set_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_FAILED, \
                "Command '%s' failed", G_STRFUNC); \
    return FALSE; \
}

static gboolean
cmd_zero(PuCommandContext *context,
         GError **error)
{
    g_return_val_if_fail(context != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (g_strv_length(pu_command_context_get_args(context)) != 0)
        cmd_error_out();
    if (pu_command_context_get_args(context)[0] != NULL)
        cmd_error_out();

    return TRUE;
}

static gboolean
cmd_one(PuCommandContext *context,
        GError **error)
{
    g_return_val_if_fail(context != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (g_strv_length(pu_command_context_get_args(context)) != 1)
        cmd_error_out();
    if (g_strcmp0(pu_command_context_get_args(context)[0], "") < 0)
        cmd_error_out();
    if (pu_command_context_get_args(context)[1] != NULL)
        cmd_error_out();

    return TRUE;
}

static gboolean
cmd_two(PuCommandContext *context,
        GError **error)
{
    g_return_val_if_fail(context != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    return TRUE;
}

static void
command_arg(void)
{
    g_autoptr(PuCommandContext) context = NULL;
    g_autoptr(GError) error = NULL;
    gchar **args_zero = gen_argv("zero");
    gchar **args_one = gen_argv("one first");
    gchar **args_two = gen_argv("two first second");
    gchar **args_zero_invalid = gen_argv("zero first_invalid second_invalid");
    gchar **args_one_invalid = gen_argv("one first second_invalid");
    gchar **args_two_invalid = gen_argv("two");

    PuCommandEntry entries[] =
        { { "zero", PU_COMMAND_ARG_NONE, cmd_zero,
            "command with zero arguments", NULL },
          { "one", PU_COMMAND_ARG_FILENAME, cmd_one,
            "command with one argument", NULL },
          { "two", PU_COMMAND_ARG_FILENAME_ARRAY, cmd_two,
            "command with two or more arguments", NULL },
          PU_COMMAND_ENTRY_NULL };

    context = pu_command_context_new();
    g_assert_nonnull(context);
    pu_command_context_add_entries(context, entries, NULL);

    pu_command_context_parse_strv(context, &args_zero, &error);
    g_assert_no_error(error);
    g_assert(pu_command_context_invoke(context, &error));
    g_assert_no_error(error);

    pu_command_context_parse_strv(context, &args_one, &error);
    g_assert_no_error(error);
    g_assert(pu_command_context_invoke(context, &error));
    g_assert_no_error(error);

    pu_command_context_parse_strv(context, &args_two, &error);
    g_assert_no_error(error);
    g_assert(pu_command_context_invoke(context, &error));
    g_assert_no_error(error);

    g_assert_false(pu_command_context_parse_strv(context, &args_zero_invalid, &error));
    g_assert_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_BAD_VALUE);
    g_clear_error(&error);

    g_assert_false(pu_command_context_parse_strv(context, &args_one_invalid, &error));
    g_assert_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_BAD_VALUE);
    g_clear_error(&error);

    g_assert_false(pu_command_context_parse_strv(context, &args_two_invalid, &error));
    g_assert_error(error, PU_COMMAND_ERROR, PU_COMMAND_ERROR_BAD_VALUE);
}

int
main(int argc,
     char *argv[])
{
    g_test_init(&argc, &argv, NULL);

#ifdef PARTUP_TEST_SRCDIR
    g_chdir(PARTUP_TEST_SRCDIR);
#endif

    g_test_add_func("/command/basic", command_basic);
    g_test_add_func("/command/help", command_help);
    g_test_add_func("/command/arg", command_arg);

    return g_test_run();
}
