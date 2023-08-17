/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_COMMAND_H
#define PARTUP_COMMAND_H

#include <glib.h>

#define PU_COMMAND_ERROR (pu_command_error_quark())

GQuark pu_command_error_quark(void);

typedef enum {
    PU_COMMAND_ERROR_UNKNOWN_COMMAND,
    PU_COMMAND_ERROR_BAD_VALUE,
    PU_COMMAND_ERROR_FAILED
} PuCommandError;

typedef struct _PuCommandContext PuCommandContext;
typedef struct _PuCommandEntry PuCommandEntry;

typedef gboolean (*PuCommandFunc)(PuCommandContext *context,
                                  GError **error);

typedef enum {
    PU_COMMAND_ARG_NONE,
    PU_COMMAND_ARG_FILENAME,
    PU_COMMAND_ARG_FILENAME_ARRAY
} PuCommandArg;

struct _PuCommandEntry {
    const gchar *name;
    PuCommandArg arg;
    PuCommandFunc func;
    const gchar *description;
    const GOptionEntry *option_entries;
};

#define PU_COMMAND_ENTRY_NULL \
    { NULL, 0, NULL, NULL, NULL }

PuCommandContext * pu_command_context_new(void);
void pu_command_context_free(PuCommandContext *context);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(PuCommandContext, pu_command_context_free)
void pu_command_context_add_entries(PuCommandContext *context,
                                    const PuCommandEntry *entries,
                                    const GOptionEntry *main_option_entries);
gchar * pu_command_context_get_help(PuCommandContext *context,
                                    const gchar *command);
gchar ** pu_command_context_get_args(PuCommandContext *context);
gboolean pu_command_context_parse(PuCommandContext *context,
                                  gint *argc,
                                  gchar ***argv,
                                  gchar ***arg_remaining,
                                  GError **error);
gboolean pu_command_context_parse_strv(PuCommandContext *context,
                                       gchar ***args,
                                       gchar ***arg_remaining,
                                       GError **error);
gboolean pu_command_context_invoke(PuCommandContext *context,
                                   GError **error);

#endif /* PARTUP_COMMAND_H */
