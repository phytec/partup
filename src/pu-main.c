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
#include "pu-utils.h"
#include "pu-version.h"

static gboolean arg_debug = FALSE;
static gboolean arg_version = FALSE;
static gboolean arg_skip_checksums = FALSE;
static gchar *arg_device = NULL;

GPtrArray *arg_remaining = NULL;

static gboolean
arg_parse_remaining(const gchar *option_name,
                    const gchar *value,
                    G_GNUC_UNUSED gpointer data,
                    GError **error)
{
    g_debug("%s(option_name=\"%s\" value=\"%s\")", __func__, option_name, value);

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

int
main(G_GNUC_UNUSED int argc,
     char **argv)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GOptionContext) context = NULL;
    g_autoptr(PuConfig) config = NULL;
    g_autoptr(PuEmmc) emmc = NULL;
    g_autofree gchar **args;
    gboolean is_mounted;
    gint api_version;
    const gchar *prog_name = g_path_get_basename(argv[0]);

    /* Support unicode filenames */
    args = g_strdupv(argv);

    context = g_option_context_new(NULL);
    g_option_context_add_main_entries(context, option_entries, NULL);
    g_option_context_set_description(context, "Report any issues at "
                                     "<https://github.com/phytec/partup>");
    if (!g_option_context_parse_strv(context, &args, &error)) {
        g_printerr("Failed parsing options: %s\n", error->message);
        return 1;
    }

    g_debug("%s", g_strjoinv(" ", (gchar **) arg_remaining->pdata));

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
    if (!pu_package_mount(arg_package, PARTUP_PACKAGE_MOUNT, &error)) {
        g_printerr("%s\n", error->message);
        return 1;
    }
    // Input files and configuration layout are now available in mounted dir
    // Assign variable, like arg_config, the right paths

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
        g_clear_error(&error);
        if (!pu_umount_all(arg_device, &error))
            g_printerr("Failed unmounting partitions being used by %s: %s\n",
                       prog_name, error->message);
        return 1;
    }

    g_free(arg_config);
    g_free(arg_device);
    g_ptr_array_free(arg_remaining, TRUE);

    return 0;
}
