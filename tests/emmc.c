/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <parted/parted.h>
#include "helper.h"
#include "pu-emmc.h"
#include "pu-error.h"

static void
emmc_simple(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(PuConfig) config = NULL;
    g_autoptr(PuEmmc) emmc = NULL;

    config = pu_config_new_from_file("config/simple.yaml", &error);
    g_assert_nonnull(config);

    g_autofree gchar *path = NULL;
    g_autoptr(GFile) mmcblk0 = NULL;
    g_autoptr(GFile) mmcblk0p1 = NULL;
    g_autoptr(GFile) mmcblk0p2 = NULL;
    path = g_dir_make_tmp("partup-XXXXXX", &error);
    g_assert_no_error(error);
    mmcblk0 = create_tmp_file("mmcblk0", path, 100 * PED_MEBIBYTE_SIZE, &error);
    mmcblk0p1 = create_tmp_file("mmcblk0p1", path, 32 * PED_MEBIBYTE_SIZE, &error);
    mmcblk0p2 = create_tmp_file("mmcblk0p2", path, 64 * PED_MEBIBYTE_SIZE, &error);

    emmc = pu_emmc_new(g_file_get_path(mmcblk0), config, NULL, FALSE, &error);
    g_assert_nonnull(emmc);
    g_assert_true(pu_flash_init_device(PU_FLASH(emmc), &error));
    g_assert_true(pu_flash_setup_layout(PU_FLASH(emmc), &error));
    g_assert_true(g_file_delete(mmcblk0, NULL, &error));
    g_assert_true(g_file_delete(mmcblk0p1, NULL, &error));
    g_assert_true(g_file_delete(mmcblk0p2, NULL, &error));
    g_assert_no_error(error);
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
