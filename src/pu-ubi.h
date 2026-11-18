/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_UBI_H
#define PARTUP_UBI_H

#include "pu-config.h"

typedef enum {
    /* Dynamic volumes are the default UBI volume type and may be resized. */
    PU_UBI_VOLUME_TYPE_DYNAMIC = 0,
    /* Static volumes have a fixed size and may be read directly by a
     * bootloader, without mapping the whole volume into memory first. */
    PU_UBI_VOLUME_TYPE_STATIC
} PuUbiVolumeType;

typedef struct _PuUbiVolumeInput {
    gchar *filename;
    gchar *md5sum;
    gchar *sha256sum;

    /* Internal members */
    gsize _size;
} PuUbiVolumeInput;
typedef struct _PuUbiVolume {
    gchar *name;
    gint64 size;
    PuUbiVolumeType type;
    gchar *filesystem;
    gchar *mkfs_extra_args;
    gboolean expand;
    GList *input;
} PuUbiVolume;

gboolean pu_ubi_parse_volumes(GHashTable *ubi,
                              const gchar *prefix,
                              GList **volumes,
                              GError **error);
void pu_ubi_volumes_free(GList *volumes);

#endif /* PARTUP_UBI_H */
