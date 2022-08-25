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
create_tmp_file(const gchar *template,
                gchar **out_file,
                gsize size,
                GError **error)
{
    g_autoptr(GFile) file = NULL;
    g_autoptr(GFileIOStream) stream = NULL;
    g_autofree guchar *buffer = NULL;

    file = g_file_new_tmp(template, &stream, error);
    g_assert_nonnull(file);
    g_assert_nonnull(stream);
    *out_file = g_file_get_path(file);
    g_debug("writing filename %s", *out_file);
    //g_assert_nonnull(out_file);
    buffer = g_new0(guchar, size);
    gsize written = 0;
    GOutputStream *output = g_io_stream_get_output_stream(G_IO_STREAM(stream));
    g_assert_true(g_output_stream_write_all(output, buffer, size, &written, NULL, error));
    g_debug("written bytes: %lu", written);
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
    create_tmp_file("partup-XXXXXX-mmcblk0", &path, 100 * PED_MEBIBYTE_SIZE, &error);
    create_tmp_file("partup-XXXXXX-mmcblk0p1", &path, 32 * PED_MEBIBYTE_SIZE, &error);
    create_tmp_file("partup-XXXXXX-mmcblk0p2", &path, 64 * PED_MEBIBYTE_SIZE, &error);

    /*emmc = pu_emmc_new(path, config, &error);
    g_assert_nonnull(emmc);
    g_assert_true(pu_flash_init_device(PU_FLASH(emmc), &error));
    g_assert_true(pu_flash_setup_layout(PU_FLASH(emmc), &error));
    g_assert_true(pu_flash_write_data(PU_FLASH(emmc), &error));*/
    //g_assert_true(g_file_delete(file, NULL, &error));
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
