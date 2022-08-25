/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <parted/parted.h>
#include "emmc.h"
#include "error.h"

static void
create_tmp_file(const gchar *filename,
                const gchar *pwd,
                gsize size,
                GError **error)
{
    g_autoptr(GFile) file = NULL;
    g_autoptr(GFileIOStream) stream = NULL;
    g_autofree guchar *buffer = NULL;

    file = g_file_get_child(pwd, filename);
    g_assert_nonnull(file);
    buffer = g_new0(guchar, size);
    g_assert_true(g_file_replace_contents(file, buffer, size, NULL, FALSE, 0, NULL, NULL, error);
    g_assert_no_error(*error);
}

static void
emmc_simple(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(PuConfig) config = NULL;
    g_autoptr(PuEmmc) emmc = NULL;

    config = pu_config_new_from_file("config/simple.yaml", &error);
    g_assert_nonnull(config);

    // TODO: Create a binary file with zeros of size 128MiB
    g_autofree gchar *path = NULL;
    path = g_dir_make_tmp("partup-XXXXXX", &error);
    g_assert_no_error(error);
    create_tmp_file("mmcblk0", path, 100 * PED_MEBIBYTE_SIZE, &error);
    create_tmp_file("mmcblk0p1", path, 32 * PED_MEBIBYTE_SIZE, &error);
    create_tmp_file("mmcblk0p2", path, 64 * PED_MEBIBYTE_SIZE, &error);

    emmc = pu_emmc_new(path, config, &error);
    g_assert_nonnull(emmc);
    g_assert_true(pu_flash_init_device(PU_FLASH(emmc), &error));
    g_assert_true(pu_flash_setup_layout(PU_FLASH(emmc), &error));
    /*g_assert_true(pu_flash_write_data(PU_FLASH(emmc), &error));*/
    g_assert_true(g_file_delete(g_strjoin("/", path, "mmcblk0")));
    g_assert_true(g_file_delete(g_strjoin("/", path, "mmcblk0p1")));
    g_assert_true(g_file_delete(g_strjoin("/", path, "mmcblk0p2")));
    g_assert_cmpint(g_rmdir(path), ==, 0);
}

int
main(int argc,
     char *argv[])
{
    g_test_init(&argc, &argv, NULL);

#ifdef PARTUP_TEST_SRCDIR
    g_chdir(PARTUP_TEST_SRCDIR);
#endif

    g_test_add_func("/emmc/simple", emmc_simple);

    return g_test_run();
}
