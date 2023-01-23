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

#define G_LOG_DOMAIN "partup-config"

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

#define PU_CONFIG_TAG_REGEX_NULL       "^(null|NULL|Null|~)$"
#define PU_CONFIG_TAG_REGEX_BOOLEAN    "^(true|True|TRUE|false|False|FALSE)$"
#define PU_CONFIG_TAG_REGEX_INTEGER_10 "^[-+]?[0-9]+$"
#define PU_CONFIG_TAG_REGEX_INTEGER_16 "^0x[0-9a-fA-F]+$"
#define PU_CONFIG_TAG_REGEX_FLOAT      "^[-+]?(\\.[0-9]+|[0-9]+(\\.[0-9]*)?)([eE][-+]?[0-9]+)?$"

G_DEFINE_TYPE_WITH_PRIVATE(PuConfig, pu_config, G_TYPE_OBJECT)

static gboolean pu_config_parse_scalar(PuConfig *config,
                                       PuConfigValue *value);
static gboolean pu_config_parse_mapping(PuConfig *config,
                                        GHashTable **mapping);
static gboolean pu_config_parse_sequence(PuConfig *config,
                                         GList **sequence);
static void pu_config_free_mapping(GHashTable *mapping);
static void pu_config_free_sequence(GList *sequence);

gboolean
pu_config_parse_scalar(PuConfig *config,
                       PuConfigValue *value)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(config);
    gchar *v = (gchar *) priv->event.data.scalar.value;
    gchar *tag = (gchar *) priv->event.data.scalar.tag;

    g_return_val_if_fail(priv->event.type == YAML_SCALAR_EVENT, FALSE);

    /* libyaml does not automatically assign a tag when none is explicitly
     * specified. Check for the correct type manually in this case. */
    if (priv->event.data.scalar.plain_implicit) {
        if (g_regex_match_simple(PU_CONFIG_TAG_REGEX_NULL, v, 0, 0)) {
            value->data.string = NULL;
            value->type = PU_CONFIG_VALUE_TYPE_NULL;
        } else if (g_regex_match_simple(PU_CONFIG_TAG_REGEX_BOOLEAN, v, 0, 0)) {
            value->data.boolean = g_str_equal(g_ascii_strdown(v, -1), "true");
            value->type = PU_CONFIG_VALUE_TYPE_BOOLEAN;
        } else if (g_regex_match_simple(PU_CONFIG_TAG_REGEX_INTEGER_10, v, 0, 0)) {
            value->data.integer = g_ascii_strtoll(v, NULL, 10);
            value->type = PU_CONFIG_VALUE_TYPE_INTEGER_10;
        } else if (g_regex_match_simple(PU_CONFIG_TAG_REGEX_INTEGER_16, v, 0, 0)) {
            value->data.integer = g_ascii_strtoll(v, NULL, 16);
            value->type = PU_CONFIG_VALUE_TYPE_INTEGER_16;
        } else if (g_regex_match_simple(PU_CONFIG_TAG_REGEX_FLOAT, v, 0, 0)) {
            value->data.number = g_ascii_strtod(v, NULL);
            value->type = PU_CONFIG_VALUE_TYPE_FLOAT;
        } else {
            value->data.string = g_strdup(v);
            value->type = PU_CONFIG_VALUE_TYPE_STRING;
        }
    } else if (priv->event.data.scalar.quoted_implicit) {
        value->data.string = g_strdup(v);
        value->type = PU_CONFIG_VALUE_TYPE_STRING;
    } else {
        if (g_str_equal(tag, YAML_STR_TAG)) {
            value->data.string = g_strdup((gchar *) priv->event.data.scalar.value);
            value->type = PU_CONFIG_VALUE_TYPE_STRING;
        } else if (g_str_equal(tag, YAML_INT_TAG)) {
            value->data.integer = g_ascii_strtoll((gchar *) priv->event.data.scalar.value, NULL, 10);
            value->type = PU_CONFIG_VALUE_TYPE_INTEGER_10;
        } else if (g_str_equal(tag, YAML_BOOL_TAG)) {
            value->data.boolean = g_str_equal(g_ascii_strdown((gchar *) priv->event.data.scalar.value, -1), "true");
            value->type = PU_CONFIG_VALUE_TYPE_BOOLEAN;
        } else {
            g_critical("Unexpected scalar tag '%s' occurred", priv->event.data.scalar.tag);
            return FALSE;
        }
    }

    g_debug("%s: value=%s type=%d", G_STRFUNC, v, value->type);

    return TRUE;
}

gboolean
pu_config_parse_mapping(PuConfig *config,
                        GHashTable **mapping)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(config);
    gchar *key;
    PuConfigValue *value;

    g_debug("%s: start", G_STRFUNC);

    g_return_val_if_fail(priv->event.type == YAML_MAPPING_START_EVENT, FALSE);
    g_return_val_if_fail(*mapping == NULL, FALSE);

    *mapping = g_hash_table_new(g_str_hash, g_str_equal);

    /* parse first key */
    g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);

    do {
        /* key */
        g_return_val_if_fail(priv->event.type == YAML_SCALAR_EVENT, FALSE);
        key = g_strdup((gchar *) priv->event.data.scalar.value);
        g_debug("%s: key=%s", G_STRFUNC, key);

        /* value */
        g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);
        value = g_new0(PuConfigValue, 1);
        switch (priv->event.type) {
        case YAML_SCALAR_EVENT:
            if (!pu_config_parse_scalar(config, value))
                return FALSE;
            g_hash_table_insert(*mapping, key, value);
            break;
        case YAML_MAPPING_START_EVENT:
            if (!pu_config_parse_mapping(config, &value->data.mapping))
                return FALSE;
            value->type = PU_CONFIG_VALUE_TYPE_MAPPING;
            g_hash_table_insert(*mapping, key, value);
            break;
        case YAML_SEQUENCE_START_EVENT:
            if (!pu_config_parse_sequence(config, &value->data.sequence))
                return FALSE;
            value->type = PU_CONFIG_VALUE_TYPE_SEQUENCE;
            g_hash_table_insert(*mapping, key, value);
            break;
        default:
            g_critical("Unexpected YAML event %d occured", priv->event.type);
            g_free(value);
            break;
        }

        /* parse next key or end of mapping */
        if (!yaml_parser_parse(&priv->parser, &priv->event))
            return FALSE;
    } while (priv->event.type != YAML_MAPPING_END_EVENT);

    g_debug("%s: end", G_STRFUNC);

    return TRUE;
}

gboolean
pu_config_parse_sequence(PuConfig *config,
                         GList **sequence)
{
    PuConfigPrivate *priv = pu_config_get_instance_private(config);
    PuConfigValue *value;

    g_debug("%s: start", G_STRFUNC);

    g_return_val_if_fail(priv->event.type == YAML_SEQUENCE_START_EVENT, FALSE);
    g_return_val_if_fail(*sequence == NULL, FALSE);

    /* parse first node of sequence */
    g_return_val_if_fail(yaml_parser_parse(&priv->parser, &priv->event), FALSE);

    do {
        value = g_new0(PuConfigValue, 1);
        switch (priv->event.type) {
        case YAML_SCALAR_EVENT:
            if (!pu_config_parse_scalar(config, value))
                return FALSE;
            *sequence = g_list_prepend(*sequence, value);
            break;
        case YAML_MAPPING_START_EVENT:
            if (!pu_config_parse_mapping(config, &value->data.mapping))
                return FALSE;
            value->type = PU_CONFIG_VALUE_TYPE_MAPPING;
            *sequence = g_list_prepend(*sequence, value);
            break;
        case YAML_SEQUENCE_START_EVENT:
            if (!pu_config_parse_sequence(config, &value->data.sequence))
                return FALSE;
            value->type = PU_CONFIG_VALUE_TYPE_SEQUENCE;
            *sequence = g_list_prepend(*sequence, value);
            break;
        default:
            g_critical("Unexpected YAML event %d occured", priv->event.type);
            g_free(value);
            break;
        }

        /* parse next node or end of sequence */
        if (!yaml_parser_parse(&priv->parser, &priv->event))
            return FALSE;
    } while (priv->event.type != YAML_SEQUENCE_END_EVENT);

    *sequence = g_list_reverse(*sequence);

    g_debug("%s: end", G_STRFUNC);

    return TRUE;
}

void
pu_config_free_mapping(GHashTable *mapping)
{
    GHashTableIter iter;
    gchar *key;
    PuConfigValue *value;

    g_hash_table_iter_init(&iter, mapping);

    while (g_hash_table_iter_next(&iter, (gpointer *) &key, (gpointer *) &value)) {
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

    g_hash_table_destroy(mapping);
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

    g_list_free(g_steal_pointer(&sequence));
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
            if (!pu_config_parse_mapping(config, &priv->root)) {
                g_set_error(error, PU_ERROR, PU_ERROR_CONFIG_PARSING_FAILED,
                            "Failed parsing with error: %s", priv->parser.problem);
                return NULL;
            }
            break;
        }
    } while (priv->event.type != YAML_DOCUMENT_END_EVENT);

    PuConfigValue *value = g_hash_table_lookup(priv->root, "api-version");
    g_return_val_if_fail(value != NULL, NULL);
    g_return_val_if_fail(value->type == PU_CONFIG_VALUE_TYPE_INTEGER_10, NULL);
    priv->api_version = value->data.integer;

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
