/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 PHYTEC Messtechnik GmbH
 */

#define G_LOG_DOMAIN "partup-ubi"

#include <glib.h>
#include <glib/gstdio.h>
#include "pu-checksum.h"
#include "pu-error.h"
#include "pu-file.h"
#include "pu-hashtable.h"
#include "pu-mount.h"
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
        volume->filesystem = NULL;
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

        /* Only dynamic volumes can hold a filesystem. Their default
         * filesystem is UBIFS, static volumes have none by default. */
        if (g_hash_table_contains(v->data.mapping, "filesystem"))
            volume->filesystem = pu_hash_table_lookup_string(v->data.mapping,
                                                             "filesystem", NULL);
        else if (volume->type == PU_UBI_VOLUME_TYPE_DYNAMIC)
            volume->filesystem = g_strdup("ubifs");

        if (volume->filesystem && !g_str_equal(volume->filesystem, "ubifs")) {
            g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                        "Filesystem type '%s' of volume '%s' is not supported",
                        volume->filesystem, volume->name);
            return FALSE;
        }

        if (volume->type == PU_UBI_VOLUME_TYPE_STATIC) {
            if (volume->expand) {
                g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                            "Static volume '%s' cannot be expanded", volume->name);
                return FALSE;
            }

            if (volume->filesystem) {
                g_set_error(error, PU_ERROR, PU_ERROR_UBI_PARSE,
                            "Static volume '%s' cannot have a filesystem",
                            volume->name);
                return FALSE;
            }
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

static gchar *
find_ubi_node(guint mtd_devnum)
{
    g_autoptr(GDir) dir = NULL;
    const gchar *name;

    dir = g_dir_open("/sys/class/ubi", 0, NULL);
    if (!dir)
        return NULL;

    while ((name = g_dir_read_name(dir)) != NULL) {
        g_autofree gchar *filename = NULL;
        gint64 value = 0;

        /* skip UBI volume devices, they are named ubi<major>_<minor> */
        if (!g_str_has_prefix(name, "ubi") || strchr(name, '_'))
            continue;

        filename = g_build_filename("/sys/class/ubi", name, "mtd_dev", NULL);
        if (!pu_file_read_int64(filename, &value, NULL))
            continue;

        if (value == (gint64) mtd_devnum)
            return g_strdup_printf("/dev/%s", name);
    }

    return NULL;
}

static gchar *
find_volume_node(const gchar *ubi_node,
                 const gchar *volume_name)
{
    g_autoptr(GDir) dir = NULL;
    g_autofree gchar *pattern = NULL;
    g_autofree gchar *basename = NULL;
    const gchar *name;

    dir = g_dir_open("/sys/class/ubi", 0, NULL);
    if (!dir)
        return NULL;

    basename = g_path_get_basename(ubi_node);
    pattern = g_strdup_printf("%s_", basename);

    while ((name = g_dir_read_name(dir)) != NULL) {
        g_autofree gchar *filename = NULL;
        g_autofree gchar *contents = NULL;

        if (!g_str_has_prefix(name, pattern))
            continue;

        filename = g_build_filename("/sys/class/ubi", name, "name", NULL);
        if (!g_file_get_contents(filename, &contents, NULL, NULL))
            continue;

        g_strchomp(contents);
        if (g_str_equal(contents, volume_name))
            return g_strdup_printf("/dev/%s", name);
    }

    return NULL;
}

gchar *
pu_ubi_get_ubi_node(guint mtd_devnum)
{
    return find_ubi_node(mtd_devnum);
}

gboolean
pu_ubi_detach(guint mtd_devnum,
              GError **error)
{
    g_autofree gchar *ubi_node = NULL;
    g_autofree gchar *cmd = NULL;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    ubi_node = find_ubi_node(mtd_devnum);
    if (!ubi_node) {
        g_debug("No UBI device attached to MTD %u", mtd_devnum);
        return TRUE;
    }

    g_debug("Detaching UBI device '%s' from MTD %u", ubi_node, mtd_devnum);

    cmd = g_strdup_printf("ubidetach /dev/ubi_ctrl -m %u", mtd_devnum);
    if (!pu_spawn_command_line_sync(cmd, error)) {
        g_prefix_error(error, "Failed detaching UBI device from MTD %u: ",
                       mtd_devnum);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_ubi_format(const gchar *mtd_path,
              GError **error)
{
    g_autofree gchar *cmd = NULL;

    g_return_val_if_fail(mtd_path != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_debug("Formatting MTD '%s' with UBI", mtd_path);

    cmd = g_strdup_printf("ubiformat %s -y -q", mtd_path);
    if (!pu_spawn_command_line_sync(cmd, error)) {
        g_prefix_error(error, "Failed formatting MTD '%s' with UBI: ",
                       mtd_path);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_ubi_attach(guint mtd_devnum,
              gchar **ubi_node,
              GError **error)
{
    g_autofree gchar *cmd = NULL;

    g_return_val_if_fail(ubi_node != NULL, FALSE);
    g_return_val_if_fail(*ubi_node == NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_debug("Attaching MTD %u to UBI", mtd_devnum);

    cmd = g_strdup_printf("ubiattach /dev/ubi_ctrl -m %u", mtd_devnum);
    if (!pu_spawn_command_line_sync(cmd, error)) {
        g_prefix_error(error, "Failed attaching MTD %u to UBI: ", mtd_devnum);
        return FALSE;
    }

    *ubi_node = find_ubi_node(mtd_devnum);
    if (*ubi_node == NULL) {
        g_set_error(error, PU_ERROR, PU_ERROR_UBI_FORMAT,
                    "Failed finding UBI device of MTD %u", mtd_devnum);
        return FALSE;
    }

    g_debug("MTD %u is now UBI device '%s'", mtd_devnum, *ubi_node);

    return TRUE;
}

gboolean
pu_ubi_create_volumes(const gchar *ubi_node,
                      GList *volumes,
                      GError **error)
{
    g_return_val_if_fail(ubi_node != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    for (GList *v = volumes; v != NULL; v = v->next) {
        PuUbiVolume *volume = v->data;
        g_autofree gchar *cmd = NULL;
        g_autofree gchar *name = NULL;
        g_autofree gchar *size = NULL;

        name = g_shell_quote(volume->name);

        if (volume->expand)
            size = g_strdup("-m");
        else
            size = g_strdup_printf("-s %" G_GINT64_FORMAT, volume->size);

        if (volume->type == PU_UBI_VOLUME_TYPE_STATIC)
            cmd = g_strdup_printf("ubimkvol %s -N %s -t static %s",
                                  ubi_node, name, size);
        else
            cmd = g_strdup_printf("ubimkvol %s -N %s %s", ubi_node, name, size);

        g_debug("Creating UBI volume '%s'", volume->name);

        if (!pu_spawn_command_line_sync(cmd, error)) {
            g_prefix_error(error, "Failed creating UBI volume '%s': ",
                           volume->name);
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
pu_ubi_mkfs_ubifs(const gchar *volume_node,
                  const gchar *extra_args,
                  GError **error)
{
    g_autofree gchar *cmd = NULL;

    g_debug("Making filesystem ubifs on UBI volume '%s'", volume_node);

    if (extra_args)
        cmd = g_strdup_printf("mkfs.ubifs %s %s", extra_args, volume_node);
    else
        cmd = g_strdup_printf("mkfs.ubifs %s", volume_node);

    if (!pu_spawn_command_line_sync(cmd, error)) {
        g_prefix_error(error, "Failed creating filesystem on UBI volume '%s': ",
                       volume_node);
        return FALSE;
    }

    return TRUE;
}

static gboolean
pu_ubi_update_volume(const gchar *volume_node,
                     const gchar *path,
                     GError **error)
{
    g_autofree gchar *cmd = NULL;

    g_debug("Writing file '%s' to UBI volume '%s'", path, volume_node);

    cmd = g_strdup_printf("ubiupdatevol %s %s", volume_node, path);
    if (!pu_spawn_command_line_sync(cmd, error)) {
        g_prefix_error(error, "Failed writing '%s' to UBI volume '%s': ",
                       path, volume_node);
        return FALSE;
    }

    return TRUE;
}

static gboolean
pu_ubi_verify_input(PuUbiVolumeInput *input,
                    const gchar *path,
                    gboolean skip_checksums,
                    GError **error)
{
    if (skip_checksums)
        return TRUE;

    if (!g_str_equal(input->md5sum, "")) {
        g_debug("Checking MD5 sum of input file '%s'", path);
        if (!pu_checksum_verify_file(path, input->md5sum, G_CHECKSUM_MD5, error))
            return FALSE;
    }

    if (!g_str_equal(input->sha256sum, "")) {
        g_debug("Checking SHA256 sum of input file '%s'", path);
        if (!pu_checksum_verify_file(path, input->sha256sum, G_CHECKSUM_SHA256,
                                     error))
            return FALSE;
    }

    return TRUE;
}

static gboolean
pu_ubi_write_volume_data(PuUbiVolume *volume,
                         const gchar *volume_node,
                         const gchar *prefix,
                         gboolean skip_checksums,
                         GError **error)
{
    for (GList *i = volume->input; i != NULL; i = i->next) {
        PuUbiVolumeInput *input = i->data;
        g_autofree gchar *path = NULL;
        g_autofree gchar *sha1 = NULL;

        path = pu_path_from_filename(input->filename, prefix, error);
        if (!path) {
            g_prefix_error(error, "Failed parsing input filename for UBI volume "
                           "'%s': ", volume->name);
            return FALSE;
        }

        if (!pu_ubi_verify_input(input, path, skip_checksums, error))
            return FALSE;

        if (!pu_ubi_update_volume(volume_node, path, error))
            return FALSE;

        if (skip_checksums)
            continue;

        sha1 = pu_checksum_new_from_file(path, 0, G_CHECKSUM_SHA1, error);
        if (!sha1) {
            g_prefix_error(error, "Failed calculating SHA1 sum of input file '%s': ",
                           input->filename);
            return FALSE;
        }

        g_debug("Verifying SHA1 sum of UBI volume '%s'", volume->name);
        if (!pu_checksum_verify_raw(volume_node, 0, input->_size, sha1,
                                    G_CHECKSUM_SHA1, error))
            return FALSE;
    }

    return TRUE;
}

static gboolean
pu_ubi_write_volume_filesystem(PuUbiVolume *volume,
                               const gchar *volume_node,
                               const gchar *prefix,
                               gboolean skip_checksums,
                               GError **error)
{
    g_autofree gchar *mount_point = NULL;

    if (!pu_ubi_mkfs_ubifs(volume_node, volume->mkfs_extra_args, error))
        return FALSE;

    if (!volume->input) {
        g_debug("No input specified. Leaving UBI volume '%s' empty", volume->name);
        return TRUE;
    }

    mount_point = pu_create_mount_point(volume->name, error);
    if (!mount_point)
        return FALSE;

    for (GList *i = volume->input; i != NULL; i = i->next) {
        PuUbiVolumeInput *input = i->data;
        g_autofree gchar *path = NULL;

        path = pu_path_from_filename(input->filename, prefix, error);
        if (!path) {
            g_prefix_error(error, "Failed parsing input filename for UBI volume "
                           "'%s': ", volume->name);
            return FALSE;
        }

        if (!pu_ubi_verify_input(input, path, skip_checksums, error))
            return FALSE;

        if (g_regex_match_simple(".tar", path, G_REGEX_CASELESS, 0)) {
            if (!pu_mount(volume_node, mount_point, volume->filesystem, NULL, error))
                return FALSE;
            if (!pu_archive_extract(path, mount_point, error)) {
                g_prefix_error(error, "Failed extracting file '%s' to UBI volume "
                               "'%s': ", input->filename, volume->name);
                return FALSE;
            }
            if (!pu_umount(mount_point, error))
                return FALSE;
        } else if (g_regex_match_simple(".ubifs$", path, 0, 0)) {
            if (!pu_ubi_update_volume(volume_node, path, error))
                return FALSE;

            /* Mount to let UBIFS resize the filesystem to the volume size. */
            if (!pu_mount(volume_node, mount_point, volume->filesystem, NULL, error))
                return FALSE;
            if (!pu_umount(mount_point, error))
                return FALSE;
        } else {
            if (!pu_mount(volume_node, mount_point, volume->filesystem, NULL, error))
                return FALSE;
            if (!pu_file_copy(path, mount_point, error)) {
                g_prefix_error(error, "Failed copying file '%s' to UBI volume "
                               "'%s': ", input->filename, volume->name);
                return FALSE;
            }
            if (!pu_umount(mount_point, error))
                return FALSE;
        }
    }

    g_rmdir(mount_point);

    return TRUE;
}

static gboolean
pu_ubi_write_volume(PuUbiVolume *volume,
                    const gchar *ubi_node,
                    const gchar *prefix,
                    gboolean skip_checksums,
                    GError **error)
{
    g_autofree gchar *volume_node = NULL;

    g_return_val_if_fail(volume != NULL, FALSE);
    g_return_val_if_fail(ubi_node != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    volume_node = find_volume_node(ubi_node, volume->name);
    if (!volume_node) {
        g_set_error(error, PU_ERROR, PU_ERROR_UBI_VERIFY,
                    "Failed finding UBI volume node of volume '%s'", volume->name);
        return FALSE;
    }

    if (volume->filesystem)
        return pu_ubi_write_volume_filesystem(volume, volume_node, prefix,
                                              skip_checksums, error);

    if (!volume->input) {
        g_debug("No input specified. Skipping UBI volume '%s'", volume->name);
        return TRUE;
    }

    return pu_ubi_write_volume_data(volume, volume_node, prefix, skip_checksums,
                                    error);
}

gboolean
pu_ubi_write_volumes(const gchar *ubi_node,
                     GList *volumes,
                     const gchar *prefix,
                     gboolean skip_checksums,
                     GError **error)
{
    g_return_val_if_fail(ubi_node != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    for (GList *v = volumes; v != NULL; v = v->next) {
        if (!pu_ubi_write_volume(v->data, ubi_node, prefix, skip_checksums, error))
            return FALSE;
    }

    return TRUE;
}
