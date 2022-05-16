/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include "flash.h"
#include "config.h"

typedef struct {
    gchar *device_path;
    PuConfig *config;
} PuFlashPrivate;

enum {
    PROP_0,
    PROP_DEVICE_PATH,
    PROP_CONFIG,
    NUM_PROPS
};
static GParamSpec *props[NUM_PROPS] = { NULL };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(PuFlash, pu_flash, G_TYPE_OBJECT)

static gboolean
pu_flash_default_init_device(PuFlash *self,
                             G_GNUC_UNUSED GError **error)
{
    g_critical("Flash of type '%s' does not implement PuFlash::init_device",
               G_OBJECT_TYPE_NAME(self));

    return FALSE;
}

static gboolean
pu_flash_default_setup_layout(PuFlash *self,
                              G_GNUC_UNUSED GError **error)
{
    g_critical("Flash of type '%s' does not implement PuFlash::setup_layout",
               G_OBJECT_TYPE_NAME(self));

    return FALSE;
}

static gboolean
pu_flash_default_write_data(PuFlash *self,
                            G_GNUC_UNUSED GError **error)
{
    g_critical("Flash of type '%s' does not implement PuFlash::write_data",
               G_OBJECT_TYPE_NAME(self));

    return FALSE;
}

static void
pu_flash_set_property(GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
    PuFlash *self = PU_FLASH(object);
    PuFlashPrivate *priv = pu_flash_get_instance_private(self);

    switch (prop_id) {
    case PROP_DEVICE_PATH:
        g_free(priv->device_path);
        priv->device_path = g_value_dup_string(value);
        break;
    case PROP_CONFIG:
        priv->config = g_value_get_pointer(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
pu_flash_get_property(GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
    PuFlash *self = PU_FLASH(object);
    PuFlashPrivate *priv = pu_flash_get_instance_private(self);

    switch (prop_id) {
    case PROP_DEVICE_PATH:
        g_value_set_string(value, priv->device_path);
        break;
    case PROP_CONFIG:
        g_value_set_pointer(value, priv->config);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
pu_flash_class_init(PuFlashClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS(class);

    class->init_device = pu_flash_default_init_device;
    class->setup_layout = pu_flash_default_setup_layout;
    class->write_data = pu_flash_default_write_data;

    object_class->set_property = pu_flash_set_property;
    object_class->get_property = pu_flash_get_property;

    props[PROP_DEVICE_PATH] =
        g_param_spec_string("device-path",
                            "Flash device path",
                            "The flash device to format and write data to",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
    props[PROP_CONFIG] =
        g_param_spec_pointer("config",
                             "Layout configuration",
                             "The layout configuration to be used for the device",
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties(object_class, NUM_PROPS, props);
}

static void
pu_flash_init(G_GNUC_UNUSED PuFlash *self)
{
}

gboolean
pu_flash_init_device(PuFlash *self,
                     GError **error)
{
    return PU_FLASH_GET_CLASS(self)->init_device(self, error);
}

gboolean
pu_flash_setup_layout(PuFlash *self,
                      GError **error)
{
    return PU_FLASH_GET_CLASS(self)->setup_layout(self, error);
}

gboolean
pu_flash_write_data(PuFlash *self,
                    GError **error)
{
    return PU_FLASH_GET_CLASS(self)->write_data(self, error);
}
