/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_CONFIG_PRIVATE_H
#define PARTUP_CONFIG_PRIVATE_H

#include <yaml.h>

/**
 * Parse a single YAML event of the configuration.
 *
 * @param self the PuConfig object containing the YAML parser and event.
 *
 * @return TRUE on success or FALSE if an error occured.
 */
gboolean pu_config_parse(PuConfig *self);

/**
 * Parse a single YAML event of the configuration and write the value as an
 * integer.
 *
 * @param self the PuConfig object containing the YAML parser and event.
 * @param value the integer value to write the result to.
 *
 * @return TRUE on success or FALSE if an error occured.
 */
gboolean pu_config_parse_int(PuConfig *self,
                             gint *value);

/**
 * Parse a single YAML event of the configuration and write the value as a
 * string.
 *
 * @param self the PuConfig object containing the YAML parser and event.
 * @param value the string value to write the result to. A new string is being
 *              allocated and must be freed with `g_free()` when no longer
 *              needed.
 *
 * @return TRUE on success or FALSE if an error occured.
 */
gboolean pu_config_parse_string(PuConfig *self,
                                gchar **value);

/**
 * Parse a single YAML mapping.
 *
 * @param config The PuConfig object containing the YAML parser and event.
 * @param mapping Pointer to a location of a GHashTable containing the key-value
 *                pairs.
 * @param valid_keys An array of C strings containing valid keys used for the
 *                   GHashTable in @a mapping.
 *
 * @return TRUE on success or FALSE if an error occured.
 */
gboolean pu_config_parse_mapping(PuConfig *config,
                                 GHashTable **mapping,
                                 const gchar * const valid_keys[]);

/**
 * Parse a sequence of YAML mappings.
 *
 * @param config The PuConfig object containing the YAML parser and event.
 * @param name The name of the mappings.
 * @param mappings Pointer to a location of a GList of GHashTables containing
 *                 the key-value pairs.
 * @param valid_keys An array of C strings containing valid keys used for the
 *                   GHashTables in @a mappings.
 *
 * @return TRUE on success or FALSE if an error occured.
 */
gboolean pu_config_parse_sequence_of_mappings(PuConfig *config,
                                              const gchar *name,
                                              GList **mappings,
                                              const gchar * const valid_keys[]);

/**
 * Get the current event type of the lastly parsed YAML event.
 *
 * @param self the PuConfig object containing the YAML parser and event.
 *
 * @return the current event type of the lastly parsed YAML event.
 */
yaml_event_type_t pu_config_get_event_type(PuConfig *self);

/**
 * Get the current event value of the lastly parsed YAML event.
 *
 * @param self the PuConfig object containing the YAML parser and event.
 *
 * @return the current event value of the lastly parsed YAML event.
 */
gpointer pu_config_get_event_value(PuConfig *self);

/**
 * Set the API version of a PuConfig instance.
 *
 * @param self the PuConfig object.
 * @param api_version the API version to set.
 */
void pu_config_set_api_version(PuConfig *self,
                               gint api_version);

#endif /* PARTUP_CONFIG_PRIVATE_H */
