/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <locale.h>
#include <parted/parted.h>
#include <unistd.h>
#include "pu-command.h"
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

GOptionContext *option_context = NULL;
GPtrArray *arg_remaining = NULL;

static gboolean
cmd_install(gchar **args,
            GError **error)
{
    g_autoptr(PuConfig) config = NULL;
    g_autofree gchar *config_path = NULL;
    g_autoptr(PuEmmc) emmc = NULL;
    gint api_version;
    gboolean is_mounted;

    if (getuid() != 0) {
        g_printerr("%s must be run as root!\n", g_get_prgname());
        return 1;
    }

    if (g_strcmp0(arg_device, "") <= 0) {
        g_printerr("No device specified!\n");
        g_print("%s", g_option_context_get_help(option_context, TRUE, NULL));
        return 1;
    }

    if (!pu_is_drive(arg_device)) {
        g_printerr("Device '%s' is not a drive!\n", arg_device);
        return 1;
    }

    if (!pu_device_mounted(arg_device, &is_mounted, error)) {
        g_printerr("Failed checking if device is in use: %s\n", (*error)->message);
        return 1;
    }

    if (is_mounted) {
        g_printerr("Device '%s' is in use!\n", arg_device);
        return 1;
    }

    // TODO: Mount squashfs image (partup package) to /run/partup
    if (!pu_package_mount(arg_package, PU_PACKAGE_PREFIX, error)) {
        g_printerr("%s\n", (*error)->message);
        return 1;
    }
    // Input files and configuration layout are now available in mounted dir
    // Assign variable, like arg_config, the right paths
    config_path = g_strdup(PU_PACKAGE_PREFIX "/layout.yaml");

    config = pu_config_new_from_file(config_path, error);
    if (config == NULL) {
        g_printerr("Failed creating configuration object for file '%s': %s\n",
                   config_path, (*error)->message);
        return 1;
    }
    api_version = pu_config_get_api_version(config);
    if (api_version > PARTUP_VERSION_MAJOR) {
        g_printerr("API version %d of configuration file is not compatible "
                   "with program version %d!\n", api_version, PARTUP_VERSION_MAJOR);
        return 1;
    }

    emmc = pu_emmc_new(arg_device, config, PU_PACKAGE_PREFIX, arg_skip_checksums, error);
    if (emmc == NULL) {
        g_printerr("Failed parsing eMMC info from config: %s\n", (*error)->message);
        return 1;
    }
    if (!pu_flash_init_device(PU_FLASH(emmc), error)) {
        g_printerr("Failed initializing device: %s\n", (*error)->message);
        return 1;
    }
    if (!pu_flash_setup_layout(PU_FLASH(emmc), error)) {
        g_printerr("Failed setting up layout on device: %s\n", (*error)->message);
        return 1;
    }
    if (!pu_flash_write_data(PU_FLASH(emmc), error)) {
        g_printerr("Failed writing data to device: %s\n", (*error)->message);
        g_clear_error(error);
        if (!pu_umount_all(arg_device, error))
            g_printerr("Failed unmounting partitions being used by %s: %s\n",
                       g_get_prgname(), (*error)->message);
        return 1;
    }

    return TRUE;
}

static gboolean
cmd_package(gchar **args,
            GError **error)
{
    return TRUE;
}

static gboolean
cmd_show(gchar **args,
         GError **error)
{
    return TRUE;
}

static gboolean
cmd_version(gchar **args,
            GError **error)
{
    g_debug("A=%s", g_strjoinv(" ", args));
    g_print("%s %s\n", g_get_prgname(), PARTUP_VERSION_STRING);
    return TRUE;
}

static gboolean
cmd_help(gchar **args,
         GError **error)
{
    g_autofree gchar *help = NULL;

    help = g_option_context_get_help(option_context, TRUE, NULL);
    g_print("%s", help);

    return TRUE;
}

static gboolean
arg_parse_remaining(const gchar *option_name,
                    const gchar *value,
                    G_GNUC_UNUSED gpointer data,
                    GError **error)
{
    g_debug("array add=%s", value);
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

static PuCommandEntry command_entries[] = {
    { "install", PU_COMMAND_ARG_FILENAME_ARRAY, cmd_install,
        "Install a package to a device" },
    { "package", PU_COMMAND_ARG_FILENAME_ARRAY, cmd_package,
        "Create a package from files" },
    { "show", PU_COMMAND_ARG_FILENAME, cmd_show,
        "List the contents of a package" },
    { "version", PU_COMMAND_ARG_NONE, cmd_version,
        "Print the program version" },
    { "help", PU_COMMAND_ARG_NONE, cmd_help,
        "Print the help options" },
    { NULL }
};
static const gchar *description = "\
Commands:\n\
  install PACKAGE DEVICE   Install a partup PACKAGE to DEVICE\n\
  package PACKAGE FILES…   Create a partup PACKAGE with the contents FILES\n\
  show PACKAGE             List the contents of a partup PACKAGE\n\
\n\
Report any issues at <https://github.com/phytec/partup>\
";

int
main(G_GNUC_UNUSED int argc,
     char **argv)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(PuCommandContext) context_cmd = NULL;
    g_autofree gchar **args;

    /* Support unicode filenames */
    setlocale(LC_ALL, "");
    args = g_strdupv(argv);

    arg_remaining = g_ptr_array_new();
    option_context = g_option_context_new(NULL);
    g_option_context_add_main_entries(option_context, option_entries, NULL);
    g_option_context_set_description(option_context, description);
    if (!g_option_context_parse_strv(option_context, &args, &error)) {
        g_printerr("Failed parsing options: %s\n", error->message);
        return 1;
    }
    g_ptr_array_add(arg_remaining, NULL);

    if (arg_debug) {
        const gchar *domains = g_getenv("G_MESSAGES_DEBUG");

        if (domains != NULL) {
            g_autofree gchar *new_domains = g_strdup_printf("%s %s", domains, g_get_prgname());
            g_setenv("G_MESSAGES_DEBUG", new_domains, TRUE);
        } else {
            g_setenv("G_MESSAGES_DEBUG", g_get_prgname(), TRUE);
        }
    }

    g_print("argv=%s\n", g_strjoinv(" ", argv));
    g_print("args=%s\n", g_strjoinv(" ", args));
    g_print("arg_remaining=%s\n", g_strjoinv(" ", (gchar **) arg_remaining->pdata));
    context_cmd = pu_command_context_new();
    pu_command_context_add_entries(context_cmd, command_entries);
    if (!pu_command_context_parse_strv(context_cmd, (gchar ***) &arg_remaining->pdata, &error)) {
        g_printerr("Failed parsing command: %s\n", error->message);
        return 1;
    }
    g_print("args(after cmd parsing)=%s\n", g_strjoinv(" ", args));

    //g_debug("%s", g_strjoinv(" ", (gchar **) arg_remaining->pdata));
    //g_debug("command: %s", (gchar *) g_ptr_array_index(arg_remaining, 0));
    if (!pu_command_context_invoke(context_cmd, &error)) {
        g_printerr("Failed invoking command: %s\n", error->message);
        return 1;
    }

    g_option_context_free(option_context);
    g_debug(G_STRLOC);
    g_free(arg_device);
    g_debug(G_STRLOC);
    g_free(arg_package);
    g_debug(G_STRLOC);
    g_ptr_array_free(arg_remaining, TRUE);
    g_debug(G_STRLOC);

    return 0;
}
