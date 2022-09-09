/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include "config.h"
#include "error.h"
#include "hashtable.h"

gchar *
pu_hash_table_lookup_string(GHashTable *hash_table,
                            const gchar *key,
                            const gchar *def)
{
    PuConfigValue *value = g_hash_table_lookup(hash_table, key);

    if (value == NULL)
        return g_strdup(def);

    return g_strdup(value->data.string);
}

gint64
pu_hash_table_lookup_int64(GHashTable *hash_table,
                           const gchar *key,
                           gint64 def)
{
    PuConfigValue *value = g_hash_table_lookup(hash_table, key);

    if (value == NULL)
        return def;

    return value->data.integer;
}

gboolean
pu_hash_table_lookup_boolean(GHashTable *hash_table,
                             const gchar *key,
                             gboolean def)
{
    PuConfigValue *value = g_hash_table_lookup(hash_table, key);

    if (value == NULL)
        return def;

    return value->data.boolean;
}

PedSector
pu_hash_table_lookup_sector(GHashTable *hash_table,
                            PedDevice *device,
                            const gchar *key,
                            PedSector def)
{
    PuConfigValue *value = g_hash_table_lookup(hash_table, key);
    PedSector sector;

    if (value == NULL)
        return def;

    if (value->type == PU_CONFIG_VALUE_TYPE_INTEGER_10 ||
        value->type == PU_CONFIG_VALUE_TYPE_INTEGER_16) {
        return value->data.integer;
    }

    if (value->type != PU_CONFIG_VALUE_TYPE_STRING ||
        !ped_unit_parse(value->data.string, device, &sector, NULL)) {
        g_warning("Failed parsing value '%s' to sectors, using default '%lld'",
                  value->data.string, def);
        return def;
    }

    return sector;
}

GList *
pu_hash_table_lookup_list(GHashTable *hash_table,
                          const gchar *key,
                          GList *def)
{
    PuConfigValue *value = g_hash_table_lookup(hash_table, key);

    if (value == NULL)
        return def;

    if (value->type != PU_CONFIG_VALUE_TYPE_SEQUENCE) {
        g_warning("Failed parsing sequence for key '%s'", key);
        return def;
    }

    return value->data.sequence;
}
