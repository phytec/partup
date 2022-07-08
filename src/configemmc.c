/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include "configemmc.h"
#include "configprivate.h"
#include "error.h"

enum {
    STATE_START,
    STATE_ACCEPT_SECTION,
    STATE_ACCEPT_API_VERSION,
    STATE_ACCEPT_DISKLABEL,
    STATE_ACCEPT_EMMC_BOOTPART,
    STATE_ACCEPT_RAW,
    STATE_ACCEPT_PARTITIONS,
    STATE_STOP,
    NUM_STATES
};

struct _PuConfigEmmc {
    PuConfig parent_instance;

    gint state;

    gchar *disklabel;
    GHashTable *bootpart;
    GList *raw;
    GList *partitions;
};

static const gchar * const pu_valid_part_keys[] = {
    "label", "type", "filesystem", "size", "offset", "input", "expand"
};
static const gchar * const pu_valid_bootpart_keys[] = {
    "enable", "input-offset", "output-offset", "input"
};
static const gchar * const pu_valid_bin_keys[] = {
    "input-offset", "output-offset", "input"
};

G_DEFINE_TYPE(PuConfigEmmc, pu_config_emmc, PU_TYPE_CONFIG)

static gboolean
pu_config_emmc_parse_start(PuConfigEmmc *self,
                           GError **error)
{
    PuConfig *parent = PU_CONFIG(self);
    yaml_event_type_t event_type;

    g_debug(G_STRFUNC);

    if (!pu_config_parse(parent)) {
        g_set_error(error, PU_ERROR, PU_ERROR_CONFIG,
                    "Failed parsing starting event");
        return FALSE;
    }

    event_type = pu_config_get_event_type(parent);
    g_debug("%s: event type %d", G_STRFUNC, event_type);
    if (event_type == YAML_MAPPING_START_EVENT) {
        self->state = STATE_ACCEPT_SECTION;
    }

    return TRUE;
}

static gboolean
pu_config_emmc_parse_section(PuConfigEmmc *self,
                             GError **error)
{
    PuConfig *parent = PU_CONFIG(self);
    yaml_event_type_t event_type;
    g_autofree gchar *value = NULL;

    g_debug(G_STRFUNC);

    if (!pu_config_parse(parent)) {
        g_set_error(error, PU_ERROR, PU_ERROR_CONFIG,
                    "Failed parsing section event");
        return FALSE;
    }

    event_type = pu_config_get_event_type(parent);
    if (event_type == YAML_MAPPING_END_EVENT) {
        g_debug("Reached end of mapping. Done!");
        self->state = STATE_STOP;
        return TRUE;
    }
    if (event_type != YAML_SCALAR_EVENT) {
        g_set_error(error, PU_ERROR, PU_ERROR_CONFIG,
                    "Expected YAML event %d (%s), got %d", YAML_SCALAR_EVENT,
                    G_STRINGIFY(YAML_SCALAR_EVENT), event_type);
        return FALSE;
    }

    value = g_strdup((const gchar *) pu_config_get_event_value(parent));
    g_debug("%s: event type %d, value %s", G_STRFUNC, event_type, value);
    if (g_str_equal(value, "api-version")) {
        self->state = STATE_ACCEPT_API_VERSION;
    } else if (g_str_equal(value, "disklabel")) {
        self->state = STATE_ACCEPT_DISKLABEL;
    } else if (g_str_equal(value, "emmc-boot-partitions")) {
        self->state = STATE_ACCEPT_EMMC_BOOTPART;
    } else if (g_str_equal(value, "raw")) {
        self->state = STATE_ACCEPT_RAW;
    } else if (g_str_equal(value, "partitions")) {
        self->state = STATE_ACCEPT_PARTITIONS;
    } else {
        g_set_error(error, PU_ERROR, PU_ERROR_CONFIG,
                    "Unkown section '%s'!", value);
        return FALSE;
    }

    return TRUE;
}

static gboolean
pu_config_emmc_parse_api_version(PuConfigEmmc *self,
                                 GError **error)
{
    PuConfig *parent = PU_CONFIG(self);
    gint api_version;

    g_debug(G_STRFUNC);

    if (!pu_config_parse_int(parent, &api_version)) {
        g_set_error(error, PU_ERROR, PU_ERROR_CONFIG,
                    "Failed parsing api-version event");
        return FALSE;
    }

    pu_config_set_api_version(parent, api_version);
    self->state = STATE_ACCEPT_SECTION;

    return TRUE;
}

static gboolean
pu_config_emmc_parse_disklabel(PuConfigEmmc *self,
                               GError **error)
{
    PuConfig *parent = PU_CONFIG(self);

    g_debug(G_STRFUNC);

    if (!pu_config_parse_string(parent, &self->disklabel)) {
        g_set_error(error, PU_ERROR, PU_ERROR_CONFIG,
                    "Failed parsing event 'disklabel'");
        return FALSE;
    }

    self->state = STATE_ACCEPT_SECTION;

    return TRUE;
}

static gboolean
pu_config_emmc_parse_emmc_bootpart(PuConfigEmmc *self,
                                   GError **error)
{
    PuConfig *parent = PU_CONFIG(self);

    g_debug(G_STRFUNC);

    if (!pu_config_parse_mapping(parent, &self->bootpart,
                                 pu_valid_bootpart_keys)) {
        g_set_error(error, PU_ERROR, PU_ERROR_CONFIG,
                    "Failed parsing mapping 'emmc-boot-partitions'");
        return FALSE;
    }

    self->state = STATE_ACCEPT_SECTION;

    return TRUE;
}

static gboolean
pu_config_emmc_parse_raw(PuConfigEmmc *self,
                         GError **error)
{
    PuConfig *parent = PU_CONFIG(self);

    g_debug(G_STRFUNC);

    if (!pu_config_parse_sequence_of_mappings(parent, "binary", &self->raw,
                                              pu_valid_bin_keys)) {
        g_set_error(error, PU_ERROR, PU_ERROR_CONFIG,
                    "Failed parsing sequence 'raw'");
        return FALSE;
    }

    self->state = STATE_ACCEPT_SECTION;

    return TRUE;
}

static gboolean
pu_config_emmc_parse_partitions(PuConfigEmmc *self,
                                GError **error)
{
    PuConfig *parent = PU_CONFIG(self);

    g_debug(G_STRFUNC);

    if (!pu_config_parse_sequence_of_mappings(parent, "partition", &self->partitions,
                                              pu_valid_part_keys)) {
        g_set_error(error, PU_ERROR, PU_ERROR_CONFIG,
                    "Failed parsing sequence 'partitions'");
        return FALSE;
    }

    self->state = STATE_ACCEPT_SECTION;

    return TRUE;
}

typedef gboolean
(* const pu_config_process_event_func[NUM_STATES])(PuConfigEmmc *self,
                                                   GError **error);

static gboolean
pu_config_emmc_process_event(PuConfigEmmc *self,
                             GError **error)
{
    static pu_config_process_event_func funcs = {
        [STATE_START] = pu_config_emmc_parse_start,
        [STATE_ACCEPT_SECTION] = pu_config_emmc_parse_section,
        [STATE_ACCEPT_API_VERSION] = pu_config_emmc_parse_api_version,
        [STATE_ACCEPT_DISKLABEL] = pu_config_emmc_parse_disklabel,
        [STATE_ACCEPT_EMMC_BOOTPART] = pu_config_emmc_parse_emmc_bootpart,
        [STATE_ACCEPT_RAW] = pu_config_emmc_parse_raw,
        [STATE_ACCEPT_PARTITIONS] = pu_config_emmc_parse_partitions,
        [STATE_STOP] = NULL
    };

    g_debug("%s: state = %d", G_STRFUNC, self->state);

    if (funcs[self->state] != NULL) {
        return (*funcs[self->state])(self, error);
    }

    return TRUE;
}

static gboolean
pu_config_emmc_read(PuConfig *config,
                    GError **error)
{
    PuConfigEmmc *self = PU_CONFIG_EMMC(config);

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    g_return_val_if_fail(PU_IS_CONFIG_EMMC(config), FALSE);

    if (!pu_config_set_parser_input(config, error)) {
        return FALSE;
    }

    do {
        if (!pu_config_emmc_process_event(self, error)) {
            return FALSE;
        }
    } while (self->state != STATE_STOP);

    return TRUE;
}

static void
pu_config_emmc_class_finalize(GObject *object)
{
    PuConfigEmmc *self = PU_CONFIG_EMMC(object);

    g_free(self->disklabel);
    g_hash_table_destroy(self->bootpart);
    g_list_free(self->raw);
    g_list_free(self->partitions);

    G_OBJECT_CLASS(pu_config_emmc_parent_class)->finalize(object);
}

static void
pu_config_emmc_class_init(PuConfigEmmcClass *class)
{
    PuConfigClass *config_class = PU_CONFIG_CLASS(class);
    GObjectClass *object_class = G_OBJECT_CLASS(class);

    config_class->read = pu_config_emmc_read;
    object_class->finalize = pu_config_emmc_class_finalize;
}

static void
pu_config_emmc_init(PuConfigEmmc *self)
{
    self->state = STATE_START;
    self->disklabel = NULL;
    self->bootpart = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    self->raw = NULL;
    self->partitions = NULL;
}

PuConfigEmmc *
pu_config_emmc_new_for_path(const gchar *path)
{
    g_return_val_if_fail(g_file_test(path, G_FILE_TEST_IS_REGULAR), NULL);

    return g_object_new(PU_TYPE_CONFIG_EMMC, "path", path, NULL);
}

gchar *
pu_config_emmc_get_disklabel(PuConfigEmmc *self)
{
    return self->disklabel;
}

GHashTable *
pu_config_emmc_get_bootpart(PuConfigEmmc *self)
{
    return self->bootpart;
}

GList *
pu_config_emmc_get_raw(PuConfigEmmc *self)
{
    return self->raw;
}

GList *
pu_config_emmc_get_partitions(PuConfigEmmc *self)
{
    return self->partitions;
}
