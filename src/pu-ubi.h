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

gchar * pu_ubi_get_ubi_node(guint mtd_devnum);
gboolean pu_ubi_detach(guint mtd_devnum,
                       GError **error);
gboolean pu_ubi_format(const gchar *mtd_path,
                       GError **error);
gboolean pu_ubi_attach(guint mtd_devnum,
                       gchar **ubi_node,
                       GError **error);
gboolean pu_ubi_create_volumes(const gchar *ubi_node,
                               GList *volumes,
                               GError **error);
gboolean pu_ubi_write_volumes(const gchar *ubi_node,
                              GList *volumes,
                              const gchar *prefix,
                              gboolean skip_checksums,
                              GError **error);

#endif /* PARTUP_UBI_H */
