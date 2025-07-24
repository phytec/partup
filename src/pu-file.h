/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2024 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_FILE_H
#define PARTUP_FILE_H

#include <glib.h>

gboolean pu_file_read_raw(const gchar *filename,
                          guchar **buffer,
                          goffset offset,
                          gssize count,
                          gsize *bytes_read,
                          GError **error);
gboolean pu_file_read_int64(const gchar *filename,
                            gint64 *out,
                            GError **error);
gboolean pu_file_copy(const gchar *src,
                      const gchar *dest,
                      GError **error);
goffset pu_file_get_size(const gchar *path,
                         GError **error);

#endif /* PARTUP_FILE_H */
