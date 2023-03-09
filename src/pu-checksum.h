/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_CHECKSUM_H
#define PARTUP_CHECKSUM_H

#include <glib.h>
#include <glib/gi18n.h>

gboolean pu_checksum_verify_file(const gchar *filename,
                                 const gchar *checksum,
                                 GChecksumType checksum_type,
                                 GError **error);

#endif /* PARTUP_CHECKSUM_H */
