/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_CONFIG_EMMC_H
#define PARTUP_CONFIG_EMMC_H

#include <glib-object.h>
#include "config.h"

/**
 * The type of PuConfigEmmc. Use `PU_IS_CONFIG_EMMC()` to check whether an
 * actual instance is of type PuConfigEmmc.
 */
#define PU_TYPE_CONFIG_EMMC pu_config_emmc_get_type()

/**
 * @struct PuConfigEmmc
 *
 * The configuration object for eMMC flash.
 */
G_DECLARE_FINAL_TYPE(PuConfigEmmc, pu_config_emmc, PU, CONFIG_EMMC, PuConfig)

/**
 * Create a new PuConfigEmmc instance from a given file path.
 *
 * @param path the path to the YAML config file, containing the layout
 *             configuration.
 *
 * @return the newly created PuConfigEmmc instance.
 */
PuConfigEmmc * pu_config_emmc_new_for_path(const gchar *path);


/**
 * Get the disk label specified in the configuration.
 *
 * @param self the eMMC configuration object.
 *
 * @return a string containing the disk label. This string is owned by the
 *         PuConfigEmmc object and must not be freed manually.
 */
gchar * pu_config_emmc_get_disklabel(PuConfigEmmc *self);

/**
 * Get the eMMC boot partition configuration.
 *
 * @param self the eMMC configuration object.
 *
 * @return a GHashTable containing the configuration of the boot partition. This
 *         GHashTable is owned by the PuConfigEmmc object @a self and must not
 *         be freed manually.
 */
GHashTable * pu_config_emmc_get_bootpart(PuConfigEmmc *self);

/**
 * Get the eMMC raw binary configurations.
 *
 * @param self the eMMC configuration object.
 *
 * @return a GList containing the configuration of multiple raw binaries. This
 *         GList is owned by the PuConfigEmmc object @a self and must not be
 *         freed manually.
 */
GList * pu_config_emmc_get_raw(PuConfigEmmc *self);

/**
 * Get the eMMC partition configurations.
 *
 * @param self the eMMC configuration object.
 *
 * @return a GList containing the configuration of multiple partitions. This
 *         GList is owned by the PuConfigEmmc object @a self and must not be
 *         freed manually.
 */
GList * pu_config_emmc_get_partitions(PuConfigEmmc *self);

#endif /* PARTUP_CONFIG_EMMC_H */
