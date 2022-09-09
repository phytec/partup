/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_UTILS_H
#define PARTUP_UTILS_H

#include <glib.h>
#include <parted/parted.h>

gboolean pu_file_copy(const gchar *src,
                      const gchar *dest,
                      GError **error);
gboolean pu_archive_extract(const gchar *filename,
                            const gchar *dest,
                            GError **error);
gboolean pu_make_filesystem(const gchar *part,
                            const gchar *type,
                            GError **error);
gboolean pu_write_raw(const gchar *input,
                      const gchar *output,
                      PedSector block_size,
                      PedSector input_offset,
                      PedSector output_offset,
                      GError **error);
gboolean pu_write_raw_bootpart(const gchar *input,
                               PedDevice *device,
                               guint bootpart,
                               PedSector input_offset,
                               PedSector output_offset,
                               GError **error);
gboolean pu_bootpart_enable(const gchar *device,
                            guint bootpart,
                            GError **error);

#endif /* PARTUP_UTILS_H */
