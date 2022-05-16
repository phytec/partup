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
G_DECLARE_DERIVABLE_TYPE(PuConfig, pu_config, PU, CONFIG, GObject)

struct _PuConfigClass {
    GObjectClass parent_class;

    gboolean (*read)(PuConfig *self,
                     GError **error);

    gpointer padding[8];
};

/**
 * Read and parse the specified configuration file of a PuConfig instance.
 *
 * @param self the configuration instance containing the path to the
 *             configuration file.
 * @param error a GError for error handling.
 *
 * @return TRUE on success or FALSE if an error occurred.
 */
gboolean pu_config_read(PuConfig *self,
                        GError **error);

/**
 * Get the API version specified in the configuration.
 *
 * @param self the configuration instance containing the API version.
 *
 * @return the API version number.
 */
gint pu_config_get_api_version(PuConfig *self);

/**
 * Prepare the YAML parser with the contents of the configuration file.
 *
 * @param self the configuration instance.
 * @param error a GError for error handling.
 *
 * @return TRUE on success or FALSE if an error occurred.
 */
gboolean pu_config_set_parser_input(PuConfig *self,
                                    GError **error);

#endif /* PARTUP_CONFIG_H */
