/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <parted/parted.h>
#include <unistd.h>
#include "pu-config.h"
#include "pu-emmc.h"
#include "pu-flash.h"
#include "pu-mount.h"
#include "pu-package.h"
#include "pu-utils.h"
#include "pu-version.h"

static gboolean arg_debug = FALSE;
static gboolean arg_version = FALSE;
static gboolean arg_skip_checksums = FALSE;
static gchar *arg_device = NULL;
static gchar *arg_package = NULL;

GPtrArray *arg_remaining = NULL;

static gboolean
arg_parse_remaining(const gchar *option_name,
                    const gchar *value,
                    G_GNUC_UNUSED gpointer data,
                    GError **error)
{
    g_ptr_array_add(arg_remaining, g_strdup(value));

    return TRUE;
}

// TODO: Add subcommand "package" for creating partup packages
// Add subcommand "show"/"inspect" for validating and showing package content

static GOptionEntry option_entries[] = {
    { "debug", 'd', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
        &arg_debug, "Print debug messages", NULL },
    { "skip-checksums", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
        &arg_skip_checksums, "Skip checksum verification for all input files", NULL },
    { "version", 'v', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
        &arg_version, "Print the program version and exit", NULL },
    { G_OPTION_REMAINING, 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_CALLBACK,
        arg_parse_remaining, NULL, NULL },
    { NULL }
};

static const gchar * const *commands = {
    "install",
    "package",
    "show"
};
static const gchar *description = "\
Commands:\n\
  install PACKAGE DEVICE   Install a partup PACKAGE to DEVICE\n\
  package PACKAGE FILES…   Create a partup PACKAGE with the contents FILES\n\
  show PACKAGE             List the contents of a partup PACKAGE\n\
\n\
Report any issues at <https://github.com/phytec/partup>\
";

static gboolean
parse_commands(GPtrArray *args,
               GError **error)
{
    gchar *cmd;

    g_return_val_if_fail(args != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!g_strv_contains(commands, g_ptr_array_index(args, 0))) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Unknown command '%s'");
        return FALSE;
    }
    cmd = g_ptr_array_index(args, 0);

    for (guint i = 0; i < args->len; i++) {
        
    }

    return TRUE;
}

int
main(G_GNUC_UNUSED int argc,
     char **argv)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GOptionContext) context = NULL;
    g_autoptr(PuConfig) config = NULL;
    g_autofree gchar *config_path = NULL;
    g_autoptr(PuEmmc) emmc = NULL;
    g_autofree gchar **args;
    gboolean is_mounted;
    gint api_version;

    /* Support unicode filenames */
    args = g_strdupv(argv);

    arg_remaining = g_ptr_array_new();
    context = g_option_context_new(NULL);
    g_option_context_add_main_entries(context, option_entries, NULL);
    g_option_context_set_description(context, description);
    if (!g_option_context_parse_strv(context, &args, &error)) {
        g_printerr("Failed parsing options: %s\n", error->message);
        return 1;
    }

    if (arg_debug) {
        const gchar *domains = g_getenv("G_MESSAGES_DEBUG");

        if (domains != NULL) {
            g_autofree gchar *new_domains = g_strdup_printf("%s %s", domains, g_get_prgname());
            g_setenv("G_MESSAGES_DEBUG", new_domains, TRUE);
        } else {
            g_setenv("G_MESSAGES_DEBUG", g_get_prgname(), TRUE);
        }
    }

    if (arg_version) {
        g_print("%s %s\n", g_get_prgname(), PARTUP_VERSION_STRING);
        return 0;
    }

    g_debug("%s", g_strjoinv(" ", (gchar **) arg_remaining->pdata));
    g_debug("command: %s", (gchar *) g_ptr_array_index(arg_remaining, 0));

    return 0;

    if (getuid() != 0) {
        g_printerr("%s must be run as root!\n", g_get_prgname());
        return 1;
    }

    /*if (g_strcmp0(arg_config, "") <= 0 || g_strcmp0(arg_device, "") <= 0) {
        g_printerr("Not enough arguments!\n");
        g_print("%s", g_option_context_get_help(context, TRUE, NULL));
        return 1;
    }*/

    if (!pu_is_drive(arg_device)) {
        g_printerr("Device '%s' is not a drive!\n", arg_device);
        return 1;
    }

    if (!pu_device_mounted(arg_device, &is_mounted, &error)) {
        g_printerr("Failed checking if device is in use: %s\n", error->message);
        return 1;
    }

    if (is_mounted) {
        g_printerr("Device '%s' is in use!\n", arg_device);
        return 1;
    }

    // TODO: Mount squashfs image (partup package) to /run/partup
    if (!pu_package_mount(arg_package, PU_PACKAGE_PREFIX, &error)) {
        g_printerr("%s\n", error->message);
        return 1;
    }
    // Input files and configuration layout are now available in mounted dir
    // Assign variable, like arg_config, the right paths
    config_path = g_strdup(PU_PACKAGE_PREFIX "/layout.yaml");

    config = pu_config_new_from_file(config_path, &error);
    if (config == NULL) {
        g_printerr("Failed creating configuration object for file '%s': %s\n",
                   config_path, error->message);
        return 1;
    }
    api_version = pu_config_get_api_version(config);
    if (api_version > PARTUP_VERSION_MAJOR) {
        g_printerr("API version %d of configuration file is not compatible "
                   "with program version %d!\n", api_version, PARTUP_VERSION_MAJOR);
        return 1;
    }

    emmc = pu_emmc_new(arg_device, config, PU_PACKAGE_PREFIX, arg_skip_checksums, &error);
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
        g_clear_error(&error);
        if (!pu_umount_all(arg_device, &error))
            g_printerr("Failed unmounting partitions being used by %s: %s\n",
                       g_get_prgname(), error->message);
        return 1;
    }

    g_free(arg_device);
    g_free(arg_package);
    g_ptr_array_free(arg_remaining, TRUE);

    return 0;
}
