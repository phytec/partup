/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2025 PHYTEC Messtechnik GmbH
 */

#define G_LOG_DOMAIN "partup-mtd"

#include "pu-mtd.h"

struct _PuMtd {
    PuFlash parent_instance;
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

PuMtd *
pu_mtd_new(const gchar *device_path,
           PuConfig *config,
           const gchar *prefix,
           gboolean skip_checksums,
           GError **error)
{
    PuMtd *self;

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

    return g_steal_pointer(&self);
}
