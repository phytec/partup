/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <fcntl.h>
#include <blkid.h>
#include <libmount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "pu-error.h"
#include "pu-utils.h"
#include "pu-mount.h"

gchar *
pu_create_mount_point(const gchar *name,
                      GError **error)
{
    gchar *mount_point = g_build_filename(PU_MOUNT_PREFIX, name, NULL);

    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    if (g_file_test(mount_point, G_FILE_TEST_IS_DIR)) {
        g_debug("Mount point '%s' already exists", mount_point);
        return mount_point;
    }

    if (g_mkdir_with_parents(mount_point, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Failed creating mount point '%s', errno %u",
                    mount_point, errno);
        return NULL;
    }

    return mount_point;
}

static GPtrArray *
pu_find_mounted(const gchar *device)
{
    blkid_probe pr;
    blkid_partlist ls;
    gint i;
    gint num_partitions;
    struct libmnt_table *tb;
    g_autoptr(GPtrArray) mounted = g_ptr_array_new_with_free_func(g_free);
    g_autoptr(GError) error = NULL;

    g_return_val_if_fail(g_strcmp0(device, "") > 0, NULL);

    pr = blkid_new_probe_from_filename(device);
    if (!pr)
        return NULL;

    ls = blkid_probe_get_partitions(pr);
    if (!ls) {
        blkid_free_probe(pr);
        return NULL;
    }
    num_partitions = blkid_partlist_numof_partitions(ls);
    if (num_partitions < 0) {
        blkid_free_probe(pr);
        return NULL;
    }
    blkid_free_probe(pr);

    tb = mnt_new_table();
    if (!tb)
        return NULL;

    if (mnt_table_parse_mtab(tb, NULL) < 0) {
        mnt_unref_table(tb);
        return NULL;
    }

    for (i = 0; i < num_partitions; i++) {
        g_autofree gchar *partname = pu_device_get_partition_path(device, i + 1, &error);
        if (!partname) {
            mnt_unref_table(tb);
            return NULL;
        }
        if (mnt_table_find_source(tb, partname, MNT_ITER_FORWARD))
            g_ptr_array_add(mounted, g_steal_pointer(&partname));
    }

    mnt_unref_table(tb);
    return g_steal_pointer(&mounted);
}

gboolean
pu_mount(const gchar *source,
         const gchar *mount_point,
         const gchar *type,
         const gchar *options,
         GError **error)
{
    gint ret;
    struct libmnt_context *ctx;

    g_return_val_if_fail(g_strcmp0(source, "") > 0, FALSE);
    g_return_val_if_fail(g_strcmp0(mount_point, "") > 0, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    ctx = mnt_new_context();
    if (!ctx) {
        g_set_error(error, PU_ERROR, PU_ERROR_MOUNT,
                    "Failed creating mount context while mounting '%s' to '%s'",
                    source, mount_point);
        return FALSE;
    }

    mnt_context_set_target(ctx, mount_point);
    mnt_context_set_source(ctx, source);
    if (g_strcmp0(type, "") > 0)
        mnt_context_set_fstype_pattern(ctx, type);
    if (g_strcmp0(options, "") > 0)
        mnt_context_append_options(ctx, options);

    ret = mnt_context_mount(ctx);
    if (ret || mnt_context_get_status(ctx) != 1) {
        g_set_error(error, PU_ERROR, PU_ERROR_MOUNT,
                    "Failed mounting '%s' to '%s'", source, mount_point);
        mnt_free_context(ctx);
        return FALSE;
    }

    mnt_free_context(ctx);
    return TRUE;
}

gboolean
pu_umount(const gchar *mount_point,
          GError **error)
{
    gint ret;
    struct libmnt_context *ctx;

    g_return_val_if_fail(g_strcmp0(mount_point, "") > 0, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    ctx = mnt_new_context();
    if (!ctx) {
        g_set_error(error, PU_ERROR, PU_ERROR_MOUNT,
                    "Failed creating mount context while unmounting '%s'",
                    mount_point);
        return FALSE;
    }
    mnt_context_set_target(ctx, mount_point);
    ret = mnt_context_umount(ctx);
    if (ret || mnt_context_get_status(ctx) != 1) {
        g_set_error(error, PU_ERROR, PU_ERROR_MOUNT,
                    "Failed unmounting '%s'", mount_point);
        mnt_free_context(ctx);
        return FALSE;
    }

    mnt_free_context(ctx);
    return TRUE;
}

gboolean
pu_umount_all(const gchar *device,
              GError **error)
{
    g_autoptr(GPtrArray) mounted = NULL;

    g_return_val_if_fail(g_strcmp0(device, "") > 0, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    mounted = pu_find_mounted(device);
    if (!mounted) {
        g_set_error(error, PU_ERROR, PU_ERROR_MOUNT,
                    "Failed finding mounted partitions of '%s'", device);
        return FALSE;
    }

    for (guint i = 0; i < mounted->len; i++) {
        if (!pu_umount(mounted->pdata[i], error))
            return FALSE;
    }

    return TRUE;
}

gboolean
pu_device_mounted(const gchar *device,
                  gboolean *is_mounted,
                  GError **error)
{
    g_autoptr(GPtrArray) mounted = NULL;

    g_return_val_if_fail(g_strcmp0(device, "") > 0, FALSE);

    mounted = pu_find_mounted(device);
    if (!mounted) {
        g_set_error(error, PU_ERROR, PU_ERROR_MOUNT,
                    "Failed checking if device '%s' is mounted", device);
        return FALSE;
    }

    *is_mounted = mounted->len > 0;

    return TRUE;
}
