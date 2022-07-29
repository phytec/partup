/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <gio/gio.h>
//#include <gmodule.h>
#include <yaml.h>
#include "config.h"
#include "configprivate.h"
#include "error.h"

typedef struct {
    yaml_parser_t parser;
    yaml_event_t event;
    gchar *contents;
    gsize contents_len;
    GHashTable *document;

    gint api_version;
} PuConfigPrivate;

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

static gboolean
pu_config_parse_scalar(PuConfig *config,
                       gpointer *value)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(config);

    g_debug(G_STRFUNC);

    g_return_val_if_fail(priv->event.type == YAML_SCALAR_EVENT, FALSE);

    if (g_str_equal(priv->event.data.scalar.tag, YAML_STR_TAG)) {
        value = g_strdup(priv->event.data.scalar.value);
    } else if (g_str_equal(priv->event.data.scalar.tag, YAML_INT_TAG)) {
        *value = g_ascii_strtoll(priv->event.data.scalar.value, NULL, 10);
    } else if (g_str_equal(priv->event.data.scalar.tag, YAML_BOOL_TAG)) {
        *value = g_str_equal(g_ascii_strdown(priv->event.data.scalar.value, -1), "true");
    } else {
        g_critical("Unexpected scalar tag '%s' occured", priv->event.data.scalar.tag);
        return FALSE;
    }

    return TRUE;
}

static gboolean
pu_config_parse_mapping(PuConfig *config,
                        GHashTable **mapping)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(config);

    g_debug(G_STRFUNC);

    g_return_val_if_fail(priv->event.type == YAML_MAPPING_START_EVENT, FALSE);
    g_return_val_if_fail(*mapping == NULL, FALSE);

    *mapping = g_hash_table_new(g_str_hash, g_str_equal);

    /* parse first key */
    g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);

    do {
        /* key */
        g_return_val_if_fail(priv->event.type == YAML_SCALAR_EVENT, FALSE);
        gchar *key = g_strdup((gchar *) priv->event.data.scalar.value);

        /* value */
        g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
        switch (priv->event.type) {
        case YAML_SCALAR_EVENT:
            gchar *value = g_strdup((gchar *) priv->event.data.scalar.value);
            g_hash_table_insert(*mapping, key, value);
            break;
        case YAML_MAPPING_START_EVENT:
            GHashTable *nested_mapping;
            pu_config_parse_mapping(config, &nested_mapping);
            g_hash_table_insert(*mapping, key, nested_mapping);
            break;
        case YAML_SEQUENCE_START_EVENT:
            GList *sequence;
            pu_config_parse_sequence(config, &sequence);
            g_hash_table_insert(*mapping, key, sequence);
            break;
        default:
            g_critical("Unexpected YAML event %d occured", priv->event.type);
            break;
        }

        /* parse next key or end of mapping */
        g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
    } while (priv->event.type != YAML_MAPPING_END_EVENT);

    return TRUE;
}

static gboolean
pu_config_parse_sequence(PuConfig *config,
                         GList **sequence)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(config);

    g_return_val_if_fail(priv->event.type == YAML_SEQUENCE_START_EVENT, FALSE);
    g_return_val_if_fail(*sequence == NULL, FALSE);

    /* parse first node of sequence */
    g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);

    do {
        switch (priv->event.type) {
        case YAML_SCALAR_EVENT:
            gchar *value = g_strdup((gchar *) priv->event.data.scalar.value);
            *sequence = g_list_prepend(*sequence, value);
            break;
        case YAML_MAPPING_START_EVENT:
            GHashTable *mapping;
            pu_config_parse_mapping(config, &mapping);
            *sequence = g_list_prepend(*sequence, mapping);
            break;
        case YAML_SEQUENCE_START_EVENT:
            GList *nested_sequence;
            pu_config_parse_sequence(config, &nested_sequence);
            *sequence = g_list_prepend(*sequence, nested_sequence);
            break;
        default:
            g_critical("Unexpected YAML event %d occured", priv->event.type);
            break;
        }

        /* parse next node or end of sequence */
        g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
    } while (priv->event.type != YAML_SEQUENCE_END_EVENT);

    *sequence = g_list_reverse(*sequence);

    return TRUE;
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
    priv->document = NULL;
}

gint
pu_config_get_api_version(PuConfig *self)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(self);

    return priv->api_version;
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

    if (!g_file_get_contents(priv->path, &priv->contents, &priv->contents_len, &error_file)) {
        g_propagate_prefixed_error(error, error_file,
                                   "Failed reading contents of file '%s'",
                                   priv->path);
        return NULL;
    }

    if (!yaml_parser_initialize(&priv->parser)) {
        g_set_error(error, PU_ERROR, PU_ERROR_CONFIG_INIT_FAILED,
                    "Failed intializing YAML parser");
        return NULL;
    }

    yaml_parser_set_input_string(&priv->parser, (guchar *) priv->contents, priv->contents_len);

    do {
        if (!yaml_parser_parse(&priv->parser, &priv->event)) {
            g_set_error(error, PU_ERROR, PU_ERROR_CONFIG_PARSING_FAILED,
                        "Failed parsing event (type %d)", priv->event.type);
            pu_config_free(config);
            return NULL;
        }
        g_debug("Parsing event type %d", priv->event.type);

        if (priv->event.type == YAML_DOCUMENT_START_EVENT) {
            continue;
        } else if (priv->event.type == YAML_MAPPING_START_EVENT) {
            pu_config_parse_mapping(config, &priv->root);
        } else {
            g_set_error(error, PU_ERROR, PU_ERROR_CONFIG_INVALID_ROOT,
                        "Invalid root element of type %d", priv->event.type);
            pu_config_free(config);
            return NULL;
        }
    } while (priv->event.type != YAML_DOCUMENT_END_EVENT);

    return g_steal_pointer(&config);
}

void
pu_config_free(PuConfig *config)
{
    g_free(priv->contents);
    /* TODO: Free individual elements of hash table first */
    g_hash_table_destroy(priv->document);

    yaml_event_delete(&priv->event);
    yaml_parser_delete(&priv->parser);
}
