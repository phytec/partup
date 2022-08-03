/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "config.h"
#include "error.h"

static void
config_simple(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(PuConfig) config = NULL;
    GHashTable *root;
    PuConfigValue *value;
    gchar *disklabel;
    GList *partitions;

    config = pu_config_new_from_file("config/simple.yaml", &error);
    g_assert_true(config != NULL);
    g_assert_true(pu_config_get_api_version(config) == 42);
    root = pu_config_get_root(config);
    g_assert_true(root != NULL);
    value = g_hash_table_lookup(root, "disklabel");
    disklabel = value->data.string;
    g_assert_true(g_str_equal(disklabel, "msdos"));
    value = g_hash_table_lookup(root, "partitions");
    partitions = value->data.sequence;
    g_assert_true(partitions != NULL);
    for (GList *p = partitions; p != NULL; p = p->next) {
        PuConfigValue *value = p->data;
        g_debug("%s", value->data.string);
    }
    g_clear_error(&error);
}

int
main(int argc,
     char *argv[])
{
    g_test_init(&argc, &argv, NULL);

#ifdef PARTUP_TEST_SRCDIR
    g_chdir(PARTUP_TEST_SRCDIR);
#endif

    g_test_add_func("/config/simple", config_simple);

    return g_test_run();
}
