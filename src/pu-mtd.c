/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2025 PHYTEC Messtechnik GmbH
 */

#define G_LOG_DOMAIN "partup-mtd"

#include <fcntl.h>
#include <glib.h>
#include <linux/blkpg.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <glib/gstdio.h>
#include "pu-error.h"
#include "pu-file.h"
#include "pu-flash.h"
#include "pu-hashtable.h"
#include "pu-utils.h"
#include "pu-mtd.h"

typedef struct _PuMtdInput {
    gchar *filename;
    gchar *md5sum;
    gchar *sha256sum;

    /* Internal members */
    gsize _size;
} PuMtdInput;
typedef struct _PuMtdPartition {
    gchar *label;
    gint64 size;
    gint64 offset;
    gboolean erase;
    PuMtdInput *input;
} PuMtdPartition;

struct _PuMtd {
    PuFlash parent_instance;

    GList *partitions;
};

G_DEFINE_TYPE(PuMtd, pu_mtd, PU_TYPE_FLASH)

static gboolean
pu_mtd_init_device(PuFlash *flash,
                   GError **error)
{
    /* TODO:
     * 1. Check for existing partitioning through device tree or commandline. If
     *    there is any, abort with an error.
     * 2. Delete any existing partitions, like "mtdpart del" does.
     */
    return TRUE;
}

static gboolean
pu_mtd_setup_layout(PuFlash *flash,
                    GError **error)
{
    /* TODO:
     * 1. Add partitions as specified in the config. See "mtdpart add".
     * 2. Erase each partition's content. See "flash_erase".
     */
    PuMtd *self = PU_MTD(flash);
    int fd;
    struct blkpg_partition part;
    struct blkpg_ioctl_arg arg;
    g_autofree gchar *device_path = NULL;

    g_object_get(flash,
                 "device-path", &device_path,
                 NULL);

    /* Add partitions */
    /*fd = g_open(device_path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Failed opening %s", device_path);
        return FALSE;
    }
    memset(&part, 0, sizeof(part));
    memset(&arg, 0, sizeof(arg));
    arg.datalen = sizeof(part);
    arg.data = &part;
    arg.op = BLKPG_ADD_PARTITION;
    part.start = 0; // TODO
    part.length = 0x80000; // TODO
    strncpy(part.devname, "tiboot3", // TODO
            sizeof(part.devname) - 1);
    part.devname[sizeof(part.devname) - 1] = '\0';
    if (ioctl(fd, BLKPG, &arg)) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Failed issuing BLKPG ioctl for new partition '%s'", "tiboot3");
        close(fd);
        return FALSE;
    }
    close(fd);*/

    /* Erase partitions' content if erase == TRUE */

    return TRUE;
}

static gboolean
pu_mtd_write_data(PuFlash *flash,
                  GError **error)
{
    /* TODO:
     * 1. Check if input binary fits target partition.
     * 2. Get length and checksum of input binary.
     * 3. Write input binary byte-wise to target partition.
     * 4. Verify checksum of written data in target partition with the one
     *    produced earlier.
     */
    return TRUE;
}

static void
pu_mtd_class_finalize(GObject *object)
{
    G_OBJECT_CLASS(pu_mtd_parent_class)->finalize(object);
}

static void
pu_mtd_class_init(PuMtdClass *class)
{
    PuFlashClass *flash_class = PU_FLASH_CLASS(class);
    GObjectClass *object_class = G_OBJECT_CLASS(class);

    flash_class->init_device = pu_mtd_init_device;
    flash_class->setup_layout = pu_mtd_setup_layout;
    flash_class->write_data = pu_mtd_write_data;

    object_class->finalize = pu_mtd_class_finalize;
}

static void
pu_mtd_init(G_GNUC_UNUSED PuMtd *self)
{
}

static gboolean
pu_mtd_parse_partitions(PuMtd *mtd,
                        GHashTable *root,
                        GError **error)
{
    PuConfigValue *value_partitions = g_hash_table_lookup(root, "partitions");
    GList *partitions;
    g_autofree gchar *path = NULL;
    g_autofree gchar *prefix = NULL;

    g_return_val_if_fail(mtd != NULL, FALSE);
    g_return_val_if_fail(root != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_object_get(mtd,
                 "prefix", &prefix,
                 NULL);

    if (!value_partitions) {
        g_debug("No entry 'partitions' found. Skipping...");
        return TRUE;
    }

    if (value_partitions->type != PU_CONFIG_VALUE_TYPE_SEQUENCE) {
        g_set_error(error, PU_ERROR, PU_ERROR_MTD_PARSE,
                    "'partitions' is not a sequence");
        return FALSE;
    }
    partitions = value_partitions->data.sequence;

    for (GList *p = partitions; p != NULL; p = p->next) {
        PuConfigValue *v = p->data;
        if (v->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
            g_set_error(error, PU_ERROR, PU_ERROR_MTD_PARSE,
                        "'partitions' does not contain sequence of mappings");
            return FALSE;
        }

        PuMtdPartition *part = g_new0(PuMtdPartition, 1);
        part->label = pu_hash_table_lookup_string(v->data.mapping, "label", NULL);
        part->size = pu_hash_table_lookup_bytes(v->data.mapping, "size", 0);
        part->offset = pu_hash_table_lookup_bytes(v->data.mapping, "offset", 0);
        part->erase = pu_hash_table_lookup_boolean(v->data.mapping, "erase", TRUE);
        if (part->size == 0) {
            g_set_error(error, PU_ERROR, PU_ERROR_MTD_PARSE,
                        "Partition is missing a size");
            return FALSE;
        }
        PuConfigValue *value_input = g_hash_table_lookup(v->data.mapping, "input");
        if (value_input) {
            if (value_input->type != PU_CONFIG_VALUE_TYPE_MAPPING) {
                g_set_error(error, PU_ERROR, PU_ERROR_MTD_PARSE,
                            "'input' of partition does not contain a mapping");
                return FALSE;
            }
            PuMtdInput *input = g_new0(PuMtdInput, 1);
            input->filename = pu_hash_table_lookup_string(value_input->data.mapping, "filename", "");
            input->md5sum = pu_hash_table_lookup_string(value_input->data.mapping, "md5sum", "");
            input->sha256sum = pu_hash_table_lookup_string(value_input->data.mapping, "sha256sum", "");

            path = pu_path_from_filename(input->filename, prefix, error);
            if (path == NULL)
                return FALSE;

            input->_size = pu_file_get_size(path, error);
            if (!input->_size)
                return FALSE;

            part->input = input;
            g_debug("Parsed partition input: filename=%s md5sum=%s sha256sum=%s",
                    input->filename, input->md5sum, input->sha256sum);

            if ((gint64) input->_size >= part->size) {
                g_set_error(error, PU_ERROR, PU_ERROR_MTD_PARSE,
                            "Input file '%s' (%ld bytes) exceeds partition size (%ld bytes)",
                            input->filename, input->_size, part->size);
                return FALSE;
            }
        }

        mtd->partitions = g_list_prepend(mtd->partitions, part);
    }

    mtd->partitions = g_list_reverse(mtd->partitions);

    return TRUE;
}

PuMtd *
pu_mtd_new(const gchar *device_path,
           PuConfig *config,
           const gchar *prefix,
           gboolean skip_checksums,
           GError **error)
{
    PuMtd *self;
    GHashTable *root;

    g_return_val_if_fail(device_path != NULL, NULL);
    g_return_val_if_fail(!g_str_equal(device_path, ""), NULL);
    g_return_val_if_fail(config != NULL, NULL);
    g_return_val_if_fail(PU_IS_CONFIG(config), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    self = g_object_new(PU_TYPE_MTD,
                        "device-path", device_path,
                        "config", config,
                        "prefix", prefix,
                        "skip-checksums", skip_checksums,
                        NULL);
    root = pu_config_get_root(config);

    if (!pu_mtd_parse_partitions(self, root, error))
        return NULL;

    return g_steal_pointer(&self);
}
