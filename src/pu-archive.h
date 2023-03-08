/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_ARCHIVE_H
#define PARTUP_ARCHIVE_H

#include <glib.h>

gboolean pu_archive_extract(const gchar *filename,
                            const gchar *dest,
                            GError **error);

#endif /* PARTUP_ARCHIVE_H */
