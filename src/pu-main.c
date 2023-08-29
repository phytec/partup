/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#define G_LOG_DOMAIN "partup"

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
#include "pu-log.h"
#include "pu-mount.h"
#include "pu-package.h"
#include "pu-utils.h"
#include "pu-version.h"

static gboolean arg_debug = FALSE;
static gchar *arg_debug_domains = NULL;
static gboolean arg_quiet = FALSE;
static gboolean arg_install_skip_checksums = FALSE;
static gchar *arg_package_directory = NULL;
static gboolean arg_package_force = FALSE;
static gboolean arg_show_size = FALSE;
static gchar **arg_remaining = NULL;

static inline gboolean
error_not_root(GError **error)
{
    g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                "%s must be run as root", g_get_prgname());
    return FALSE;
}

static inline gboolean
error_out(const gchar *mountpoint)
{
    pu_package_umount(mountpoint, NULL);
    return FALSE;
}

static gboolean
cmd_install(PuCommandContext *context,
            GError **error)
{
    g_autoptr(PuConfig) config = NULL;
    g_autofree gchar *mount_path = NULL;
    g_autofree gchar *config_path = NULL;
    g_autofree gchar *package_path = NULL;
    g_autofree gchar *device_path = NULL;
    g_autoptr(PuEmmc) emmc = NULL;
    gchar **args;
    gint api_version;
    gboolean is_mounted;

    if (getuid() != 0)
        return error_not_root(error);

    args = pu_command_context_get_args(context);
    package_path = g_strdup(args[0]);
    device_path = g_strdup(args[1]);

    if (args[2]) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Too many arguments");
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

    if (!pu_package_mount(package_path, &mount_path, &config_path, error))
        return FALSE;

    config = pu_config_new_from_file(config_path, error);
    if (config == NULL) {
        g_prefix_error(error, "Failed creating configuration object for file '%s': ",
                       config_path);
        return error_out(mount_path);
    }
    api_version = pu_config_get_api_version(config);
    if (api_version > PARTUP_VERSION_MAJOR) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "API version %d of configuration file is not compatible "
                    "with program version %d", api_version, PARTUP_VERSION_MAJOR);
        return error_out(mount_path);
    }

    emmc = pu_emmc_new(device_path, config, mount_path,
                       arg_install_skip_checksums, error);
    if (emmc == NULL) {
        g_prefix_error(error, "Failed parsing eMMC info from config: ");
        return error_out(mount_path);
    }
    if (!pu_flash_init_device(PU_FLASH(emmc), error)) {
        g_prefix_error(error, "Failed initializing device: ");
        return error_out(mount_path);
    }
    if (!pu_flash_setup_layout(PU_FLASH(emmc), error)) {
        g_prefix_error(error, "Failed setting up layout on device: ");
        return error_out(mount_path);
    }
    if (!pu_flash_write_data(PU_FLASH(emmc), error)) {
        g_prefix_error(error, "Failed writing data to device: ");
        pu_umount_all(device_path, NULL);
        return error_out(mount_path);
    }

    return pu_package_umount(mount_path, error);
}

static gboolean
cmd_package(PuCommandContext *context,
            GError **error)
{
    g_autofree gchar *package = NULL;
    gchar **args;

    args = pu_command_context_get_args(context);

    if (!g_path_is_absolute(args[0]) && arg_package_directory)
        package = g_build_filename(g_get_current_dir(), args[0], NULL);
    else
        package = g_strdup(args[0]);

    if (arg_package_directory) {
        if (g_chdir(arg_package_directory) < 0) {
            g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                        "Cannot change to directory '%s'", arg_package_directory);
            return FALSE;
        }
    }

    return pu_package_create(&args[1], package, arg_package_force, error);
}

static gboolean
cmd_show(PuCommandContext *context,
         GError **error)
{
    gchar **args;

    if (getuid() != 0)
        return error_not_root(error);

    args = pu_command_context_get_args(context);

    return pu_package_list_contents(args[0], arg_show_size, error);
}

static gboolean
cmd_version(G_GNUC_UNUSED PuCommandContext *context,
            G_GNUC_UNUSED GError **error)
{
    g_message("%s %s", g_get_prgname(), PARTUP_VERSION_STRING);

    return TRUE;
}

static GOptionEntry option_entries_main[] = {
    { "debug", 'd', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
        &arg_debug, "Print debug messages for all modules", NULL },
    { "debug-domains", 'D', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING,
        &arg_debug_domains, "Comma separated list of modules to enable debug output for",
        "DEBUG_DOMAINS" },
    { "quiet", 'q', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
        &arg_quiet, "Only print error messages", NULL },
    { NULL }
};

static GOptionEntry option_entries_install[] = {
    { "skip-checksums", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
        &arg_install_skip_checksums, "Skip checksum verification for all input files", NULL },
    { G_OPTION_REMAINING, 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING_ARRAY,
        &arg_remaining, NULL, "install PACKAGE DEVICE" },
    { NULL }
};

static GOptionEntry option_entries_package[] = {
    { "directory", 'C', G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME,
        &arg_package_directory, "Change to DIR before creating the package", "DIR" },
    { "force", 'f', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
        &arg_package_force, "Overwrite any existing package", NULL },
    { G_OPTION_REMAINING, 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING_ARRAY,
        &arg_remaining, NULL, "package PACKAGE FILES…" },
    { NULL }
};

static GOptionEntry option_entries_show[] = {
    { "size", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
        &arg_show_size, "Print the size of each file", NULL },
    { G_OPTION_REMAINING, 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING_ARRAY,
        &arg_remaining, NULL, "show PACKAGE" },
    { NULL }
};

static GOptionEntry option_entries_version[] = {
    { G_OPTION_REMAINING, 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING_ARRAY,
        &arg_remaining, NULL, "version" },
    { NULL }
};

static PuCommandEntry command_entries[] = {
    { "install", PU_COMMAND_ARG_FILENAME_ARRAY, cmd_install,
        "Install a package to a device", option_entries_install },
    { "package", PU_COMMAND_ARG_FILENAME_ARRAY, cmd_package,
        "Create a package from files", option_entries_package },
    { "show", PU_COMMAND_ARG_FILENAME, cmd_show,
        "List the contents of a package", option_entries_show },
    { "version", PU_COMMAND_ARG_NONE, cmd_version,
        "Print the program version", option_entries_version },
    PU_COMMAND_ENTRY_NULL
};

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

    pu_log_init();

    context_cmd = pu_command_context_new();
    pu_command_context_add_entries(context_cmd, command_entries, option_entries_main);
    if (!pu_command_context_parse_strv(context_cmd, &args, &arg_remaining, &error)) {
        g_critical("%s", error->message);
        return 1;
    }

    pu_log_set_debug_domains(arg_quiet, arg_debug, arg_debug_domains);

    if (!pu_command_context_invoke(context_cmd, &error)) {
        g_critical("%s", error->message);
        return 1;
    }

    return 0;
}
