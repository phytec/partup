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
#include "pu-error.h"
#include "pu-flash.h"
#include "pu-mount.h"
#include "pu-package.h"
#include "pu-utils.h"
#include "pu-version.h"

static gboolean arg_debug = FALSE;
static gboolean arg_version = FALSE;
static gboolean arg_skip_checksums = FALSE;

GPtrArray *arg_remaining = NULL;

static gboolean
cmd_install(gchar **args,
            G_GNUC_UNUSED GOptionContext *option_context,
            GError **error)
{
    g_autoptr(PuConfig) config = NULL;
    g_autofree gchar *config_path = NULL;
    g_autofree gchar *package_path = g_strdup(args[0]);
    g_autofree gchar *device_path = g_strdup(args[1]);
    g_autoptr(PuEmmc) emmc = NULL;
    gint api_version;
    gboolean is_mounted;

    if (getuid() != 0) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "%s must be run as root", g_get_prgname());
        return FALSE;
    }

    if (g_strcmp0(device_path, "") <= 0) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "No device specified");
        return FALSE;
    }

    if (!pu_is_drive(device_path)) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Device '%s' is not a drive", device_path);
        return FALSE;
    }

    if (!pu_device_mounted(device_path, &is_mounted, error)) {
        g_prefix_error(error, "Failed checking if device is in use: ");
        return FALSE;
    }

    if (is_mounted) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Device '%s' is in use", device_path);
        return FALSE;
    }

    // TODO: Mount squashfs image (partup package) to /run/partup/package
    if (!pu_create_mount_point(PU_PACKAGE_BASENAME, error))
        return FALSE;
    if (!pu_mount(package_path, PU_PACKAGE_PREFIX, "squashfs", "loop,ro", error))
        return FALSE;
    // Input files and configuration layout are now available in mounted dir
    // Assign variable, like arg_config, the right paths
    config_path = g_strdup(PU_PACKAGE_LAYOUT_FILE);

    config = pu_config_new_from_file(config_path, error);
    if (config == NULL) {
        g_prefix_error(error, "Failed creating configuration object for file '%s': ",
                       config_path);
        goto error_out;
    }
    api_version = pu_config_get_api_version(config);
    if (api_version > PARTUP_VERSION_MAJOR) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "API version %d of configuration file is not compatible "
                    "with program version %d", api_version, PARTUP_VERSION_MAJOR);
        goto error_out;
    }

    emmc = pu_emmc_new(device_path, config, PU_PACKAGE_PREFIX, arg_skip_checksums, error);
    if (emmc == NULL) {
        g_prefix_error(error, "Failed parsing eMMC info from config: ");
        goto error_out;
    }
    if (!pu_flash_init_device(PU_FLASH(emmc), error)) {
        g_prefix_error(error, "Failed initializing device: ");
        goto error_out;
    }
    if (!pu_flash_setup_layout(PU_FLASH(emmc), error)) {
        g_prefix_error(error, "Failed setting up layout on device: ");
        goto error_out;
    }
    if (!pu_flash_write_data(PU_FLASH(emmc), error)) {
        g_prefix_error(error, "Failed writing data to device: ");
        pu_umount_all(device_path, NULL);
        goto error_out;
    }

    return pu_umount(PU_PACKAGE_PREFIX, error);

error_out:
    pu_umount(PU_PACKAGE_PREFIX, NULL);
    return FALSE;
}

static gboolean
cmd_package(gchar **args,
            G_GNUC_UNUSED GOptionContext *option_context,
            GError **error)
{
    g_autofree gchar *package = NULL;
    g_autoptr(GPtrArray) input_files = NULL;

    package = g_strdup(args[0]);
    input_files = g_ptr_array_new();
    for (gint i = 1; args[i] != NULL; i++)
        g_ptr_array_add(input_files, g_strdup(args[i]));
    g_ptr_array_add(input_files, NULL);

    return pu_package_create(input_files, package, error);
}

static gboolean
cmd_show(gchar **args,
         G_GNUC_UNUSED GOptionContext *option_context,
         GError **error)
{
    return pu_package_list_contents(args[0], error);
}

static gboolean
cmd_version(G_GNUC_UNUSED gchar **args,
            G_GNUC_UNUSED GOptionContext *option_context,
            G_GNUC_UNUSED GError **error)
{
    g_print("%s %s\n", g_get_prgname(), PARTUP_VERSION_STRING);

    return TRUE;
}

static gboolean
cmd_help(G_GNUC_UNUSED gchar **args,
         GOptionContext *option_context,
         G_GNUC_UNUSED GError **error)
{
    g_autofree gchar *help = NULL;

    help = g_option_context_get_help(option_context, TRUE, NULL);
    g_print("%s", help);

    return TRUE;
}

static gboolean
arg_parse_remaining(G_GNUC_UNUSED const gchar *option_name,
                    const gchar *value,
                    G_GNUC_UNUSED gpointer data,
                    G_GNUC_UNUSED GError **error)
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
  version                  Print the program version\n\
  help                     Show help options\n\
\n\
Report any issues at <https://github.com/phytec/partup>\
";

int
main(G_GNUC_UNUSED int argc,
     char **argv)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(PuCommandContext) context_cmd = NULL;
    g_autoptr(GOptionContext) option_context = NULL;
    g_autofree gchar **args;
    g_autofree gchar *default_cmd = NULL;

    /* Support unicode filenames */
    setlocale(LC_ALL, "");
    args = g_strdupv(argv);

    arg_remaining = g_ptr_array_new();
    option_context = g_option_context_new("COMMAND");
    g_option_context_add_main_entries(option_context, option_entries, NULL);
    g_option_context_set_description(option_context, description);
    if (!g_option_context_parse_strv(option_context, &args, &error)) {
        g_printerr("Failed parsing options: %s\n", error->message);
        return 1;
    }
    if (arg_remaining->len == 0) {
        default_cmd = g_strdup(command_entries[0].name);
        g_warning("Not specifying a command is deprecated, defaulting to '%s'", default_cmd);
        g_ptr_array_add(arg_remaining, default_cmd);
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

    context_cmd = pu_command_context_new(option_context);
    pu_command_context_add_entries(context_cmd, command_entries);
    if (!pu_command_context_parse_strv(context_cmd, (gchar ***) &arg_remaining->pdata, &error)) {
        g_printerr("Failed parsing command: %s\n", error->message);
        return 1;
    }

    if (!pu_command_context_invoke(context_cmd, &error)) {
        g_printerr("%s\n", error->message);
        return 1;
    }

    g_ptr_array_free(arg_remaining, TRUE);

    return 0;
}
