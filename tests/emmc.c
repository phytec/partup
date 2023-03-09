/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <parted/parted.h>
#include "pu-emmc.h"
#include "pu-error.h"

static GFile *
create_tmp_file(const gchar *filename,
                const gchar *pwd,
                gsize size,
                GError **error)
{
    GFile *file;
    g_autoptr(GFileIOStream) stream = NULL;
    g_autofree gchar *buffer = NULL;

    file = g_file_new_build_filename(pwd, filename, NULL);
    g_assert_nonnull(file);
    buffer = g_new0(gchar, size);
    g_assert_true(g_file_replace_contents(file, buffer, size, NULL, FALSE, 0, NULL, NULL, error));
    g_assert_no_error(*error);

    return g_steal_pointer(&file);
}

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
