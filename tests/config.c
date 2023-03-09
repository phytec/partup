/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "pu-config.h"
#include "pu-error.h"

static void
config_simple(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(PuConfig) config = NULL;
    GHashTable *root;
    PuConfigValue *value;
    gchar *disklabel;

    config = pu_config_new_from_file("config/simple.yaml", &error);
    g_assert_nonnull(config);
    g_assert_cmpint(pu_config_get_api_version(config), ==, 0);
    root = pu_config_get_root(config);
    g_assert_nonnull(root);
    value = g_hash_table_lookup(root, "disklabel");
    g_assert_nonnull(value);
    g_assert_cmpint(value->type, ==, PU_CONFIG_VALUE_TYPE_STRING);
    disklabel = value->data.string;
    g_assert_true(g_str_equal(disklabel, "msdos"));
    g_clear_error(&error);
}

static void
config_types(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(PuConfig) config = NULL;
    GHashTable *root;
    GHashTableIter root_iter;
    gchar *key;
    PuConfigValue *value;
    PuConfigValueType type;

    config = pu_config_new_from_file("config/types.yaml", &error);
    g_assert_nonnull(config);
    root = pu_config_get_root(config);
    g_assert_nonnull(root);
    g_hash_table_iter_init(&root_iter, root);
    while (g_hash_table_iter_next(&root_iter, (gpointer *) &key, (gpointer *) &value)) {
        if (value->type != PU_CONFIG_VALUE_TYPE_SEQUENCE)
            continue;

        if (g_str_equal(key, "nulls"))
            type = PU_CONFIG_VALUE_TYPE_NULL;
        else if (g_str_has_prefix(key, "integers10"))
            type = PU_CONFIG_VALUE_TYPE_INTEGER_10;
        else if (g_str_has_prefix(key, "integers16"))
            type = PU_CONFIG_VALUE_TYPE_INTEGER_16;
        else if (g_str_has_prefix(key, "booleans"))
            type = PU_CONFIG_VALUE_TYPE_BOOLEAN;
        else if (g_str_has_prefix(key, "floats"))
            type = PU_CONFIG_VALUE_TYPE_FLOAT;
        else if (g_str_has_prefix(key, "strings"))
            type = PU_CONFIG_VALUE_TYPE_STRING;
        else
            continue;

        for (GList *node = value->data.sequence; node != NULL; node = node->next) {
            PuConfigValue *node_value = node->data;
            g_assert_cmpint(node_value->type, ==, type);
        }
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
    g_test_add_func("/config/types", config_types);

    return g_test_run();
}
