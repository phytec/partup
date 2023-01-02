/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <parted/parted.h>
#include <unistd.h>
#include "config.h"
#include "emmc.h"
#include "flash.h"
#include "mount.h"
#include "utils.h"
#include "version.h"

static gboolean arg_debug = FALSE;
static gboolean arg_version = FALSE;
static gboolean arg_skip_checksums = FALSE;
static gchar *arg_config = NULL;
static gchar *arg_device = NULL;
static gchar *arg_prefix = NULL;

static gboolean
arg_parse_remaining(const gchar *option_name,
                    const gchar *value,
                    G_GNUC_UNUSED gpointer data,
                    GError **error)
{
    g_debug("%s(option_name=\"%s\" value=\"%s\")", __func__, option_name, value);

    if (arg_device == NULL) {
        arg_device = g_strdup(value);
    } else {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                    "%s", "Excess values in remaining arguments!");
        return FALSE;
    }

    return TRUE;
}

static GOptionEntry option_entries[] = {
    { "config", 'c', G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME,
        &arg_config, "Layout configuration file in YAML format", "CONFIG" },
    { "debug", 'd', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
        &arg_debug, "Print debug messages", NULL },
    { "prefix", 'p', G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME,
        &arg_prefix, "Path to prefix all file URIs with in the layout configuration",
        "PREFIX" },
    { "skip-checksums", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
        &arg_skip_checksums, "Skip checksum verification for all input files", NULL },
    { "version", 'v', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
        &arg_version, "Print the program version and exit", NULL },
    { G_OPTION_REMAINING, 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_CALLBACK,
        (void *) arg_parse_remaining, NULL, NULL },
    { NULL }
};

int
main(G_GNUC_UNUSED int argc,
     char **argv)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GOptionContext) context = NULL;
    g_autoptr(PuConfig) config = NULL;
    g_autoptr(PuEmmc) emmc = NULL;
    g_autofree gchar **args;
    gint api_version;
    const gchar *prog_name = g_path_get_basename(argv[0]);

    /* Support unicode filenames */
    args = g_strdupv(argv);

    context = g_option_context_new("-c CONFIG DEVICE");
    g_option_context_add_main_entries(context, option_entries, NULL);
    g_option_context_set_description(context, "Report any issues at "
                                     "<https://github.com/phytec/partup>");
    if (!g_option_context_parse_strv(context, &args, &error)) {
        g_printerr("Failed parsing options: %s\n", error->message);
        return 1;
    }

    if (arg_debug) {
        const gchar *domains = g_getenv("G_MESSAGES_DEBUG");

        if (domains != NULL) {
            g_autofree gchar *new_domains = g_strdup_printf("%s %s", domains, prog_name);
            g_setenv("G_MESSAGES_DEBUG", new_domains, TRUE);
        } else {
            g_setenv("G_MESSAGES_DEBUG", prog_name, TRUE);
        }
    }

    if (arg_version) {
        g_print("%s %s\n", prog_name, PARTUP_VERSION_STRING);
        return 0;
    }

    if (getuid() != 0) {
        g_printerr("%s must be run as root!\n", prog_name);
        return 1;
    }

    if (g_strcmp0(arg_config, "") <= 0 || g_strcmp0(arg_device, "") <= 0) {
        g_printerr("Not enough arguments!\n");
        g_print("%s", g_option_context_get_help(context, TRUE, NULL));
        return 1;
    }

    if (pu_device_mounted(arg_device)) {
        g_printerr("Device '%s' is in use!\n", arg_device);
        return 1;
    }

    if (!g_str_has_suffix(arg_config, ".yaml") &&
        !g_str_has_suffix(arg_config, ".yml")) {
        g_printerr("Configuration file does not seem to be a YAML file!\n");
        return 1;
    }

    config = pu_config_new_from_file(arg_config, &error);
    if (config == NULL) {
        g_printerr("Failed creating configuration object for file '%s': %s\n",
                   arg_config, error->message);
        return 1;
    }
    api_version = pu_config_get_api_version(config);
    if (api_version > PARTUP_VERSION_MAJOR) {
        g_printerr("API version %d of configuration file is not compatible "
                   "with program version %d!\n", api_version, PARTUP_VERSION_MAJOR);
        return 1;
    }

    emmc = pu_emmc_new(arg_device, config, arg_prefix, arg_skip_checksums, &error);
    if (emmc == NULL) {
        g_printerr("Failed parsing eMMC info from config: %s\n", error->message);
        return 1;
    }
    if (!pu_flash_init_device(PU_FLASH(emmc), &error)) {
        g_printerr("Failed initializing device: %s\n", error->message);
        return 1;
    }
    if (!pu_flash_setup_layout(PU_FLASH(emmc), &error)) {
        g_printerr("Failed setting up layout on device: %s\n", error->message);
        return 1;
    }
    if (!pu_flash_write_data(PU_FLASH(emmc), &error)) {
        g_printerr("Failed writing data to device: %s\n", error->message);
        return 1;
    }

    g_free(arg_config);
    g_free(arg_device);

    return 0;
}
