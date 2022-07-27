/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <gio/gio.h>
#include <gmodule.h>
#include <parted/parted.h>
#include <yaml.h>
#include "config.h"
#include "configprivate.h"
#include "error.h"

typedef struct {
    yaml_parser_t parser;
    yaml_event_t event;
    gchar *contents;
    gsize contents_len;

    gchar *path;
    gint api_version;
} PuConfigPrivate;

enum {
    PROP_0,
    PROP_PATH,
    PROP_API_VERSION,
    NUM_PROPS
};
static GParamSpec *props[NUM_PROPS] = { NULL };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(PuConfig, pu_config, G_TYPE_OBJECT)

gboolean
pu_config_parse(PuConfig *self)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(self);

    if (!yaml_parser_parse(&priv->parser, &priv->event)) {
        g_critical("%s: %s (error %d)", G_STRFUNC, priv->parser.problem,
                   priv->parser.error);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_config_parse_int(PuConfig *self,
                    gint *value)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(self);

    if (!yaml_parser_parse(&priv->parser, &priv->event)) {
        g_critical("%s: %s (error %d)", G_STRFUNC, priv->parser.problem,
                   priv->parser.error);
        return FALSE;
    }
    *value = g_ascii_strtoll(pu_config_get_event_value(self), NULL, 10);

    return TRUE;
}

gboolean
pu_config_parse_string(PuConfig *self,
                       gchar **value)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(self);

    if (!yaml_parser_parse(&priv->parser, &priv->event)) {
        g_critical("%s: %s (error %d)", G_STRFUNC, priv->parser.problem,
                   priv->parser.error);
        return FALSE;
    }
    *value = g_strdup(pu_config_get_event_value(self));

    return TRUE;
}

/*gboolean
pu_config_parse_input(PuConfig *self,
                      GHashTable **mapping,
                      const gchar * const valid_input_keys[])
{
    PuConfigPrivate *priv = pu_config_get_instance_private(self);
    g_autofree gchar *key = NULL;

    g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
    g_return_val_if_fail(priv->event.type == YAML_SEQUENCE_START_EVENT, FALSE);

    g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);

    do {
        g_return_val_if_fail(priv->event.type == YAML_MAPPING_START_EVENT, FALSE);
    } while (priv->event.type != YAML_SEQUENCE_END_EVENT);

    return TRUE;
}*/

/* Recursively use this function. When using for parsing input, set valid_keys
 * to the valid input keys and valid_input_keys to NULL to prevent further
 * recursion. */
gboolean
pu_config_parse_mapping(PuConfig *config,
                        GHashTable **mapping,
                        const gchar * const valid_keys[],
                        const gchar * const valid_input_keys[])
{
    PuConfigPrivate *priv = pu_config_get_instance_private(config);
    g_autofree gchar *key = NULL;

    /* Parse first mapping */
    g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
    g_return_val_if_fail(priv->event.type == YAML_MAPPING_START_EVENT, FALSE);

    /* Parse first key */
    g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
    g_debug("new hash table");

    do {
        /* key */
        if (priv->event.type != YAML_SCALAR_EVENT) {
            g_critical("Expected YAML_SCALAR_EVENT (%d), got %d!",
                       YAML_SCALAR_EVENT, priv->event.type);
            //g_hash_table_destroy(*mapping);
            return FALSE;
        }

        if (g_strv_contains(valid_keys, (char *) priv->event.data.scalar.value)) {
            key = g_strdup((char *) priv->event.data.scalar.value);
        } else {
            g_warning("Unknown key \"%s\"!", priv->event.data.scalar.value);
            /* skip value of unknown key */
            /* Maybe we should not consume the value, in case there is no
             * value. */
            g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
            continue;
        }

        /* value */
        g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
        if (g_str_equal(priv->event.data.scalar.value, "input")) {
            g_debug("Inserting sequence of 'input' mappings");
            GList *input_list = NULL;
            pu_config_parse_sequence_of_mappings(config, "input", &input_list,
                                                 pu_valid_input_keys);
        } else {
            g_debug("Inserting key-value pair into hash table");
            g_hash_table_insert(*mapping, g_strdup(key),
                                g_strdup((char *) priv->event.data.scalar.value));
        }

        g_debug("Done inserting");

        /* Parse next key or end of mapping */
        g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
    } while (priv->event.type != YAML_MAPPING_END_EVENT);

    return TRUE;
}

/* TODO: should be "parse sequence of keyed mappings"? Or simply remove the key
 * "partition" and "binary" for a true sequence of mappings (without the key).
 */
gboolean
pu_config_parse_sequence_of_mappings(PuConfig *config,
                                     const gchar *name,
                                     GList **mappings,
                                     const gchar * const valid_keys[])
{
    PuConfigPrivate *priv = pu_config_get_instance_private(config);
    g_autofree gchar *key = NULL;
    GHashTable *hash_table;

    g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
    g_return_val_if_fail(priv->event.type == YAML_SEQUENCE_START_EVENT, FALSE);

    /* Parse first mapping */
    g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);

    do {
        g_return_val_if_fail(priv->event.type == YAML_MAPPING_START_EVENT, FALSE);
        g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
        g_return_val_if_fail(priv->event.type == YAML_SCALAR_EVENT, FALSE);
        g_return_val_if_fail(g_str_equal(priv->event.data.scalar.value, name), FALSE);
        g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
        g_return_val_if_fail(priv->event.type == YAML_MAPPING_START_EVENT, FALSE);

        /* Parse first key */
        g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
        hash_table = g_hash_table_new(g_str_hash, g_str_equal);
        g_debug("new hash table");

        do {
            /* key */
            if (priv->event.type != YAML_SCALAR_EVENT) {
                g_critical("Expected YAML_SCALAR_EVENT (%d), got %d!",
                           YAML_SCALAR_EVENT, priv->event.type);
                g_hash_table_destroy(hash_table);
                return FALSE;
            }

            if (g_strv_contains(valid_keys, (char *) priv->event.data.scalar.value)) {
                key = g_strdup((char *) priv->event.data.scalar.value);
            } else {
                g_warning("Unknown key '%s'!", priv->event.data.scalar.value);
                /* skip value of unknown key */
                /* Maybe we should not consume the value, in case there is no
                 * value. */
                g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
                continue;
            }

            /* value */
            g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
            g_debug("Inserting key-value pair into hash table");
            g_hash_table_insert(hash_table, g_strdup(key),
                                g_strdup((char *) priv->event.data.scalar.value));
            g_debug("Done inserting");

            /* Parse next key */
            g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
        } while (priv->event.type != YAML_MAPPING_END_EVENT);

        g_debug("Prepending hash table to mappings");
        *mappings = g_list_prepend(*mappings, hash_table);
        g_debug("Added new mapping of type '%s'", name);

        g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
        g_return_val_if_fail(priv->event.type == YAML_MAPPING_END_EVENT, FALSE);

        /* Parse next mapping or end of sequence */
        g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
    } while (priv->event.type != YAML_SEQUENCE_END_EVENT);

    *mappings = g_list_reverse(*mappings);
    g_debug("Done adding all mappings of type '%s'", name);

    return TRUE;
}

yaml_event_type_t
pu_config_get_event_type(PuConfig *self)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(self);

    return priv->event.type;
}

gpointer
pu_config_get_event_value(PuConfig *self)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(self);

    return priv->event.data.scalar.value;
}

void
pu_config_set_api_version(PuConfig *self,
                          gint api_version)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(self);

    priv->api_version = api_version;
}

static gboolean
pu_config_default_read(PuConfig *self,
                       G_GNUC_UNUSED GError **error)
{
    g_critical("Config of type '%s' does not implement PuConfig::read",
               G_OBJECT_TYPE_NAME(self));

    return FALSE;
}

static void
pu_config_set_property(GObject *object,
                       guint prop_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
    PuConfig *self = PU_CONFIG(object);
    PuConfigPrivate *priv = pu_config_get_instance_private(self);

    switch (prop_id) {
    case PROP_PATH:
        g_free(priv->path);
        priv->path = g_value_dup_string(value);
        break;
    case PROP_API_VERSION:
        priv->api_version = g_value_get_int(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
pu_config_get_property(GObject *object,
                       guint prop_id,
                       GValue *value,
                       GParamSpec *pspec)
{
    PuConfig *self = PU_CONFIG(object);
    PuConfigPrivate *priv = pu_config_get_instance_private(self);

    switch (prop_id) {
    case PROP_PATH:
        g_value_set_string(value, priv->path);
        break;
    case PROP_API_VERSION:
        g_value_set_int(value, priv->api_version);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
pu_config_class_finalize(GObject *object)
{
    PuConfig *self = PU_CONFIG(object);
    PuConfigPrivate *priv = pu_config_get_instance_private(self);

    g_free(priv->contents);
    g_free(priv->path);

    yaml_event_delete(&priv->event);
    yaml_parser_delete(&priv->parser);

    G_OBJECT_CLASS(pu_config_parent_class)->finalize(object);
}

static void
pu_config_class_init(PuConfigClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS(class);

    class->read = pu_config_default_read;

    object_class->set_property = pu_config_set_property;
    object_class->get_property = pu_config_get_property;
    object_class->finalize = pu_config_class_finalize;

    props[PROP_PATH] =
        g_param_spec_string("path",
                            "Configuration file path",
                            "The path to the layout configuration file",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
    props[PROP_API_VERSION] =
        g_param_spec_pointer("api-version",
                             "API version",
                             "The API version of the layout configuration file",
                             G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, NUM_PROPS, props);
}

static void
pu_config_init(G_GNUC_UNUSED PuConfig *self)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(self);

    priv->contents = NULL;
    priv->contents_len = 0;
}

gboolean
pu_config_read(PuConfig *self,
               GError **error)
{
    return PU_CONFIG_GET_CLASS(self)->read(self, error);
}

gint
pu_config_get_api_version(PuConfig *self)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(self);

    return priv->api_version;
}

gboolean
pu_config_set_parser_input(PuConfig *self,
                           GError **error)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(self);

    g_debug(G_STRFUNC);

    if (!yaml_parser_initialize(&priv->parser)) {
        g_set_error(error, PU_ERROR, PU_ERROR_CONFIG,
                    "Failed intializing YAML parser");
        return FALSE;
    }

    if (!g_file_get_contents(priv->path, &priv->contents, &priv->contents_len, NULL)) {
        g_set_error(error, PU_ERROR, PU_ERROR_CONFIG,
                    "Failed reading contents of file '%s'", priv->path);
        return FALSE;
    }

    yaml_parser_set_input_string(&priv->parser, (guchar *) priv->contents, priv->contents_len);

    return TRUE;
}
