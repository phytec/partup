/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <gio/gio.h>
#include <gmodule.h>
#include <yaml.h>
#include "config.h"
#include "error.h"

typedef struct _PuConfigPrivate PuConfigPrivate;

struct _PuConfigPrivate {
    yaml_parser_t parser;
    yaml_event_t event;
    gchar *contents;
    gsize contents_len;

    GHashTable *root;
    gint api_version;
};
struct _PuConfig {
    GObject parent;
};

enum {
    PROP_0,
    PROP_API_VERSION,
    NUM_PROPS
};
static GParamSpec *props[NUM_PROPS] = { NULL };

/* TODO: Idea:
 * - PuConfig just serializes the YAML document to GLib types.
 * - The flash type then converts and checks the content of PuConfig and uses
 *   that for its flash definition.
 * - Subclassing PuConfig to e.g. PuConfigEmmc may not be needed, as the
 *   conversion happens inside the flash type PuEmmc directly.
 */
G_DEFINE_TYPE_WITH_PRIVATE(PuConfig, pu_config, G_TYPE_OBJECT)

/*static gboolean pu_config_parse_scalar(PuConfig *config,
                                       gpointer *value)*/
static gboolean pu_config_parse_mapping(PuConfig *config,
                                        GHashTable **mapping);
static gboolean pu_config_parse_sequence(PuConfig *config,
                                         GList **sequence);
static void pu_config_free_mapping(GHashTable *mapping);
static void pu_config_free_sequence(GList *sequence);

/*gboolean
pu_config_parse_scalar(PuConfig *config,
                       gpointer *value)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(config);

    g_debug(G_STRFUNC);

    g_return_val_if_fail(priv->event.type == YAML_SCALAR_EVENT, FALSE);

    if (g_str_equal(priv->event.data.scalar.tag, YAML_STR_TAG)) {
        value = (gpointer) g_strdup((gchar *) priv->event.data.scalar.value);
    } else if (g_str_equal(priv->event.data.scalar.tag, YAML_INT_TAG)) {
        *value = g_ascii_strtoll((gchar *) priv->event.data.scalar.value, NULL, 10);
    } else if (g_str_equal(priv->event.data.scalar.tag, YAML_BOOL_TAG)) {
        *value = g_str_equal(g_ascii_strdown((gchar *) priv->event.data.scalar.value, -1), "true");
    } else {
        g_critical("Unexpected scalar tag '%s' occured", priv->event.data.scalar.tag);
        return FALSE;
    }

    return TRUE;
}*/

gboolean
pu_config_parse_mapping(PuConfig *config,
                        GHashTable **mapping)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(config);
    gchar *key;
    PuConfigValue *value;

    g_debug(G_STRFUNC);

    g_return_val_if_fail(priv->event.type == YAML_MAPPING_START_EVENT, FALSE);
    g_return_val_if_fail(*mapping == NULL, FALSE);

    *mapping = g_hash_table_new(g_str_hash, g_str_equal);

    /* parse first key */
    g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);

    do {
        /* key */
        g_return_val_if_fail(priv->event.type == YAML_SCALAR_EVENT, FALSE);
        key = g_strdup((gchar *) priv->event.data.scalar.value);

        /* value */
        g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
        value = g_new0(PuConfigValue, 1);
        switch (priv->event.type) {
        case YAML_SCALAR_EVENT:
            g_debug("mapping: string");
            value->data.string = g_strdup((gchar *) priv->event.data.scalar.value);
            value->type = PU_CONFIG_VALUE_TYPE_STRING;
            g_hash_table_insert(*mapping, key, value);
            break;
        case YAML_MAPPING_START_EVENT:
            pu_config_parse_mapping(config, &value->data.mapping);
            value->type = PU_CONFIG_VALUE_TYPE_MAPPING;
            g_hash_table_insert(*mapping, key, value);
            break;
        case YAML_SEQUENCE_START_EVENT:
            pu_config_parse_sequence(config, &value->data.sequence);
            value->type = PU_CONFIG_VALUE_TYPE_SEQUENCE;
            g_hash_table_insert(*mapping, key, value);
            break;
        default:
            g_critical("Unexpected YAML event %d occured", priv->event.type);
            g_free(value);
            break;
        }

        /* parse next key or end of mapping */
        g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
    } while (priv->event.type != YAML_MAPPING_END_EVENT);

    return TRUE;
}

gboolean
pu_config_parse_sequence(PuConfig *config,
                         GList **sequence)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(config);
    PuConfigValue *value;

    g_debug(G_STRFUNC);

    g_return_val_if_fail(priv->event.type == YAML_SEQUENCE_START_EVENT, FALSE);
    g_return_val_if_fail(*sequence == NULL, FALSE);

    /* parse first node of sequence */
    g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);

    do {
        value = g_new0(PuConfigValue, 1);
        switch (priv->event.type) {
        case YAML_SCALAR_EVENT:
            g_debug("sequence: string");
            value->data.string = g_strdup((gchar *) priv->event.data.scalar.value);
            value->type = PU_CONFIG_VALUE_TYPE_STRING;
            *sequence = g_list_prepend(*sequence, value);
            break;
        case YAML_MAPPING_START_EVENT:
            pu_config_parse_mapping(config, &value->data.mapping);
            value->type = PU_CONFIG_VALUE_TYPE_MAPPING;
            *sequence = g_list_prepend(*sequence, value);
            break;
        case YAML_SEQUENCE_START_EVENT:
            pu_config_parse_sequence(config, &value->data.sequence);
            value->type = PU_CONFIG_VALUE_TYPE_SEQUENCE;
            *sequence = g_list_prepend(*sequence, value);
            break;
        default:
            g_critical("Unexpected YAML event %d occured", priv->event.type);
            g_free(value);
            break;
        }

        /* parse next node or end of sequence */
        g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
    } while (priv->event.type != YAML_SEQUENCE_END_EVENT);

    *sequence = g_list_reverse(*sequence);

    return TRUE;
}

void
pu_config_free_mapping(GHashTable *mapping)
{
    GHashTableIter iter;
    gchar *key;
    PuConfigValue *value;

    g_hash_table_iter_init(&iter, mapping);

    while (g_hash_table_iter_next(&iter, (gpointer) &key, (gpointer) &value)) {
        g_free(key);
        switch (value->type) {
        case PU_CONFIG_VALUE_TYPE_STRING:
            g_free(value->data.string);
            break;
        case PU_CONFIG_VALUE_TYPE_MAPPING:
            pu_config_free_mapping(value->data.mapping);
            break;
        case PU_CONFIG_VALUE_TYPE_SEQUENCE:
            pu_config_free_sequence(value->data.sequence);
            break;
        default:
            break;
        }
        g_free(value);
    }
}

void
pu_config_free_sequence(GList *sequence)
{
    PuConfigValue *value;

    for (GList *node = sequence; node != NULL; node = node->next) {
        value = node->data;
        switch (value->type) {
        case PU_CONFIG_VALUE_TYPE_STRING:
            g_free(value->data.string);
            break;
        case PU_CONFIG_VALUE_TYPE_MAPPING:
            pu_config_free_mapping(value->data.mapping);
            break;
        case PU_CONFIG_VALUE_TYPE_SEQUENCE:
            pu_config_free_sequence(value->data.sequence);
            break;
        default:
            break;
        }
        g_free(value);
    }
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
    /* TODO: Free individual elements of hash table first */
    pu_config_free_mapping(priv->root);

    yaml_event_delete(&priv->event);
    yaml_parser_delete(&priv->parser);

    G_OBJECT_CLASS(pu_config_parent_class)->finalize(object);
}

static void
pu_config_class_init(PuConfigClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS(class);

    object_class->set_property = pu_config_set_property;
    object_class->get_property = pu_config_get_property;
    object_class->finalize = pu_config_class_finalize;

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
    priv->api_version = -1;
    priv->root = NULL;
}

PuConfig *
pu_config_new_from_file(const gchar *filename,
                        GError **error)
{
    g_autoptr(GError) error_file = NULL;

    g_return_val_if_fail(g_file_test(filename, G_FILE_TEST_IS_REGULAR), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    PuConfig *config = g_object_new(PU_TYPE_CONFIG, NULL);
    PuConfigPrivate *priv = pu_config_get_instance_private(config);

    if (!g_file_get_contents(filename, &priv->contents, &priv->contents_len, &error_file)) {
        g_propagate_prefixed_error(error, error_file,
                                   "Failed reading contents of file '%s'",
                                   filename);
        return NULL;
    }

    if (!yaml_parser_initialize(&priv->parser)) {
        g_set_error(error, PU_ERROR, PU_ERROR_CONFIG_INIT_FAILED,
                    "Failed intializing YAML parser");
        g_object_unref(config);
        return NULL;
    }

    yaml_parser_set_input_string(&priv->parser, (guchar *) priv->contents, priv->contents_len);

    do {
        if (!yaml_parser_parse(&priv->parser, &priv->event)) {
            g_set_error(error, PU_ERROR, PU_ERROR_CONFIG_PARSING_FAILED,
                        "Failed parsing event (type %d)", priv->event.type);
            g_object_unref(config);
            return NULL;
        }
        g_debug("Parsing event type %d", priv->event.type);

        if (priv->event.type == YAML_MAPPING_START_EVENT) {
            /* We expect a mapping as the root element in the config */
            pu_config_parse_mapping(config, &priv->root);
            break;
        }
    } while (priv->event.type != YAML_DOCUMENT_END_EVENT);

    g_debug("Finished parsing");

    PuConfigValue *api_version_value = g_hash_table_lookup(priv->root, "api-version");
    priv->api_version = g_ascii_strtoll(api_version_value->data.string, NULL, 10);

    return g_steal_pointer(&config);
}

gint
pu_config_get_api_version(PuConfig *config)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(config);

    return priv->api_version;
}

GHashTable *
pu_config_get_root(PuConfig *config)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(config);

    return priv->root;
}
