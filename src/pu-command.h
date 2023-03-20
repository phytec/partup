/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_COMMAND_H
#define PARTUP_COMMAND_H

#include <glib.h>

typedef struct _PuCommandContext PuCommandContext;
typedef struct _PuCommandEntry PuCommandEntry;

typedef enum {
    PU_COMMAND_ARG_NONE,
    PU_COMMAND_ARG_FILENAME,
    PU_COMMAND_ARG_FILENAME_ARRAY
} PuCommandArg;

struct PuCommandEntry {
    const gchar *name;
    GCommandArg arg;
    gpointer arg_data;
    const gchar *description;
};

#endif /* PARTUP_COMMAND_H */
