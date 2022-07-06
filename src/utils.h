/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_UTILS_H
#define PARTUP_UTILS_H

#include <glib.h>
#include <parted/parted.h>

gboolean pu_copy_file(const gchar *filename,
                      const gchar *dest);
gboolean pu_archive_extract(const gchar *filename,
                            const gchar *dest);
gboolean pu_make_filesystem(const gchar *part,
                            const gchar *type);
gboolean pu_write_raw(const gchar *input,
                      const gchar *output,
                      PedSector block_size,
                      PedSector input_offset,
                      PedSector output_offset);
gboolean pu_write_raw_bootpart(const gchar *input,
                               PedDevice *device,
                               guint bootpart,
                               PedSector input_offset,
                               PedSector output_offset);
gboolean pu_bootpart_enable(const gchar *device,
                            guint bootpart);
gchar * pu_hash_table_lookup_string(GHashTable *hash_table,
                                    const gchar *key,
                                    const gchar *def);
gint64 pu_hash_table_lookup_int64(GHashTable *hash_table,
                                  const gchar *key,
                                  gint64 def);
gboolean pu_hash_table_lookup_boolean(GHashTable *hash_table,
                                      const gchar *key,
                                      gboolean def);
PedSector pu_hash_table_lookup_sector(GHashTable *hash_table,
                                      PedDevice *device,
                                      const gchar *key,
                                      PedSector def);

#endif /* PARTUP_UTILS_H */
