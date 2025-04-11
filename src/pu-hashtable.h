/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_HASHTABLE_H
#define PARTUP_HASHTABLE_H

#include <glib.h>
#include <parted/parted.h>

gchar * pu_hash_table_lookup_string(GHashTable *hash_table,
                                    const gchar *key,
                                    const gchar *def);
gint64 pu_hash_table_lookup_int64(GHashTable *hash_table,
                                  const gchar *key,
                                  gint64 def);
gboolean pu_hash_table_lookup_boolean(GHashTable *hash_table,
                                      const gchar *key,
                                      gboolean def);
gint64 pu_hash_table_lookup_bytes(GHashTable *hash_table,
                                  const gchar *key,
                                  gint64 def);
PedSector pu_hash_table_lookup_sector(GHashTable *hash_table,
                                      PedDevice *device,
                                      const gchar *key,
                                      PedSector def);
GList * pu_hash_table_lookup_list(GHashTable *hash_table,
                                  const gchar *key,
                                  GList *def);

#endif /* PARTUP_HASHTABLE_H */
