/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_FLASH_H
#define PARTUP_FLASH_H

#include <glib-object.h>

/**
 * The type of PuFlash. Use `PU_IS_FLASH()` to check whether an actual instance
 * is of type PuFlash.
 */
#define PU_TYPE_FLASH pu_flash_get_type()

/**
 * @struct PuFlash
 * @brief The base object used for abstracting flash devices.
 *
 * PuFlash is the base object used for specific implementations of flash
 * devices. An example can be found for eMMC flash with PuEmmc.
 *
 * Implementations of PuFlash use three different functions representing the
 * different stages of initializing, formatting and writing a flash device. The
 * implementation of these functions are flash device specific and may vary
 * depending on its type.
 */
G_DECLARE_DERIVABLE_TYPE(PuFlash, pu_flash, PU, FLASH, GObject)

struct _PuFlashClass {
    GObjectClass parent_class;

    gboolean (*init_device)(PuFlash *self,
                            GError **error);
    gboolean (*setup_layout)(PuFlash *self,
                             GError **error);
    gboolean (*write_data)(PuFlash *self,
                           GError **error);

    gpointer padding[8];
};

/**
 * Initialize the flash device.
 *
 * Initialize the flash device, e.g. set up the MBR or GPT of a harddisk.
 *
 * @param self the PuFlash instance.
 * @param error a GError used for error handling.
 *
 * @return TRUE on success or FALSE if an error occurred.
 */
gboolean pu_flash_init_device(PuFlash *self,
                              GError **error);

/**
 * Set up the layout of the flash device.
 *
 * Set up the layout of the flash device, e.g. create partitions on a harddisk
 * specified by the configuration file.
 *
 * @param self the PuFlash instance.
 * @param error a GError used for error handling.
 *
 * @return TRUE on success or FALSE if an error occurred.
 */
gboolean pu_flash_setup_layout(PuFlash *self,
                               GError **error);

/**
 * Write input data to the flash device.
 *
 * Write the input data as it was specified in the layout configuration file.
 *
 * @param self the PuFlash instance.
 * @param error a GError used for error handling.
 *
 * @return TRUE on success or FALSE if an error occurred.
 */
gboolean pu_flash_write_data(PuFlash *self,
                             GError **error);

#endif /* PARTUP_FLASH_H */
