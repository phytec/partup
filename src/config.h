/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_CONFIG_H
#define PARTUP_CONFIG_H

#include <glib-object.h>

/**
 * The type of PuConfig. Use `PU_IS_CONFIG()` to check whether an actual
 * instance is of type PuConfig.
 */
#define PU_TYPE_CONFIG pu_config_get_type()

/**
 * @struct PuConfig
 * @brief The configuration base object.
 *
 * PuConfig is the base object used for flash specific implementations of
 * layout configuration files. An example can be found for eMMC flash with
 * PuConfigEmmc.
 *
 * Implementations of PuConfig use the function `read()` to process and parse
 * the contents of the configuration file. The implementation of `read()` is
 * dependent on the flash type and supported features of the corresponding
 * configuration file for that flash type.
 */
G_DECLARE_FINAL_TYPE(PuConfig, pu_config, PU, CONFIG, GObject)

PuConfig * pu_config_new_from_file(const gchar *filename,
                                   GError **error);

void pu_config_free(PuConfig *config);

/**
 * Get the API version specified in the configuration.
 *
 * @param self the configuration instance containing the API version.
 *
 * @return the API version number.
 */
gint pu_config_get_api_version(PuConfig *config);

#endif /* PARTUP_CONFIG_H */
