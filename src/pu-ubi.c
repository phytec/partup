/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 PHYTEC Messtechnik GmbH
 */

#define G_LOG_DOMAIN "partup-ubi"

#include <glib.h>
#include "pu-error.h"
#include "pu-file.h"
#include "pu-hashtable.h"
#include "pu-utils.h"
#include "pu-ubi.h"

static void
pu_ubi_volume_input_free(PuUbiVolumeInput *input)
{
    if (!input)
        return;

    g_free(input->filename);
    g_free(input->md5sum);
    g_free(input->sha256sum);
    g_free(input);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(PuUbiVolumeInput, pu_ubi_volume_input_free)

static void
pu_ubi_volume_free(PuUbiVolume *volume)
{
    if (!volume)
        return;

    g_free(volume->name);
    g_free(volume->filesystem);
    g_free(volume->mkfs_extra_args);
    for (GList *i = volume->input; i != NULL; i = i->next)
        pu_ubi_volume_input_free(i->data);
    g_list_free(g_steal_pointer(&volume->input));
    g_free(volume);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(PuUbiVolume, pu_ubi_volume_free)

void
pu_ubi_volumes_free(GList *volumes)
{
    for (GList *v = volumes; v != NULL; v = v->next)
        pu_ubi_volume_free(v->data);

    g_list_free(volumes);
}

static gboolean
pu_ubi_parse_volume_input(GHashTable *volume,
                          const gchar *prefix,
                          GList **input,
                          GError **error)
{
    PuConfigValue *value_input = g_hash_table_lookup(volume, "input");
    GList *input_list;
    g_autofree gchar *name = NULL;

    g_return_val_if_fail(volume != NULL, FALSE);
    g_return_val_if_fail(input != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    name = pu_hash_table_lookup_string(volume, "name", NULL);

    if (!value_input) {
        g_debug("No entry 'input' found. Skipping...");
        return TRUE;
    }

    if (value_input->type != PU_CONFIG_VALUE_TYPE_SEQUENCE) {
        g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                    "'input' of volume '%s' does not contain a sequence",
                    name);
        return FALSE;
    }
    input_list = value_input->data.sequence;

    for (GList *i = input_list; i != NULL; i = i->next) {
        PuConfigValue *v = i->data;
        g_autoptr(PuUbiVolumeInput) in = NULL;
        g_autofree gchar *path = NULL;
        goffset size;

        if (v->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
            g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                        "'input' of volume '%s' does not contain sequence "
                        "of mappings", name);
            return FALSE;
        }

        in = g_new0(PuUbiVolumeInput, 1);
        in->filename = pu_hash_table_lookup_string(v->data.mapping, "filename", "");
        in->md5sum = pu_hash_table_lookup_string(v->data.mapping, "md5sum", "");
        in->sha256sum = pu_hash_table_lookup_string(v->data.mapping, "sha256sum", "");

        path = pu_path_from_filename(in->filename, prefix, error);
        if (path == NULL) {
            g_prefix_error(error, "Failed parsing input filename for volume '%s': ",
                           name);
            return FALSE;
        }

        size = pu_file_get_size(path, error);
        if (size == 0) {
            g_prefix_error(error, "Failed retrieving file size of input '%s' "
                           "for volume '%s': ", in->filename, name);
            return FALSE;
        }
        in->_size = size;

        g_debug("Parsed volume input: filename=%s md5sum=%s sha256sum=%s",
                in->filename, in->md5sum, in->sha256sum);

        *input = g_list_prepend(*input, g_steal_pointer(&in));
    }

    *input = g_list_reverse(*input);

    return TRUE;
}

gboolean
pu_ubi_parse_volumes(GHashTable *ubi,
                     const gchar *prefix,
                     GList **volumes,
                     GError **error)
{
    PuConfigValue *value_volumes = g_hash_table_lookup(ubi, "volumes");
    GList *volumes_list;

    g_return_val_if_fail(ubi != NULL, FALSE);
    g_return_val_if_fail(volumes != NULL, FALSE);
    g_return_val_if_fail(*volumes == NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!value_volumes) {
        g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                    "'ubi' is missing a 'volumes' sequence");
        return FALSE;
    }

    if (value_volumes->type != PU_CONFIG_VALUE_TYPE_SEQUENCE) {
        g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                    "'volumes' is not a sequence");
        return FALSE;
    }
    volumes_list = value_volumes->data.sequence;

    if (!volumes_list) {
        g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                    "'volumes' does not contain any volumes");
        return FALSE;
    }

    for (GList *p = volumes_list; p != NULL; p = p->next) {
        PuConfigValue *v = p->data;
        g_autoptr(PuUbiVolume) volume = NULL;
        g_autofree gchar *type_str = NULL;

        if (v->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
            g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                        "'volumes' does not contain sequence of mappings");
            return FALSE;
        }

        volume = g_new0(PuUbiVolume, 1);
        volume->name = pu_hash_table_lookup_string(v->data.mapping, "name", NULL);
        volume->size = pu_hash_table_lookup_bytes(v->data.mapping, "size", 0);
        volume->filesystem = pu_hash_table_lookup_string(v->data.mapping,
                                                         "filesystem", "ubifs");
        volume->mkfs_extra_args = pu_hash_table_lookup_string(v->data.mapping,
                                                              "mkfs-extra-args", NULL);
        volume->expand = pu_hash_table_lookup_boolean(v->data.mapping, "expand", FALSE);

        if (!volume->name) {
            g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                        "Volume is missing a name");
            return FALSE;
        }

        type_str = pu_hash_table_lookup_string(v->data.mapping, "type", "dynamic");
        if (g_str_equal(type_str, "dynamic")) {
            volume->type = PU_UBI_VOLUME_TYPE_DYNAMIC;
        } else if (g_str_equal(type_str, "static")) {
            volume->type = PU_UBI_VOLUME_TYPE_STATIC;
        } else {
            g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                        "Volume '%s' of invalid type '%s' specified",
                        volume->name, type_str);
            return FALSE;
        }

        if (volume->filesystem && !g_str_equal(volume->filesystem, "ubifs")) {
            g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                        "Filesystem type '%s' of volume '%s' is not supported",
                        volume->filesystem, volume->name);
            return FALSE;
        }

        if (volume->size == 0 && !volume->expand) {
            g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                        "Volume '%s' is missing a size", volume->name);
            return FALSE;
        }

        if (volume->expand && p->next) {
            g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                        "Only the last volume can be expanded");
            return FALSE;
        }

        if (!pu_ubi_parse_volume_input(v->data.mapping, prefix, &volume->input, error))
            return FALSE;

        /* Expanding volumes have no size yet and are not checked. */
        if (volume->size > 0) {
            for (GList *i = volume->input; i != NULL; i = i->next) {
                const PuUbiVolumeInput *in = i->data;

                if ((gint64) in->_size > volume->size) {
                    g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                                "Input file '%s' (%" G_GSIZE_FORMAT " bytes) "
                                "exceeds size of volume '%s' "
                                "(%" G_GINT64_FORMAT " bytes)",
                                in->filename, in->_size, volume->name,
                                volume->size);
                    return FALSE;
                }
            }
        }

        g_debug("Parsed volume: name=%s type=%s filesystem=%s "
                "mkfs-extra-args=%s size=%" G_GINT64_FORMAT " expand=%s",
                volume->name, type_str,
                volume->filesystem ? volume->filesystem : "null",
                volume->mkfs_extra_args, volume->size,
                volume->expand ? "true" : "false");

        *volumes = g_list_prepend(*volumes, g_steal_pointer(&volume));
    }

    *volumes = g_list_reverse(*volumes);

    return TRUE;
}
