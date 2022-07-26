/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "checksum.h"
#include "error.h"

#define RANDOM_BIN_SHA256SUM "39218ea5b78ae328a8168bcaa7a419cfb6b0c68f0994a80e3473c2b642002a07"
#define RANDOM_BIN_MD5SUM    "2d28eafb1bbda719986b430af2eb963f"

static void
checksum_good(void)
{
    g_autoptr(GError) error = NULL;

    g_assert_true(pu_checksum_verify_file("data/random.bin", RANDOM_BIN_SHA256SUM,
                                          G_CHECKSUM_SHA256, &error));
    g_assert_no_error(error);

    g_assert_true(pu_checksum_verify_file("data/random.bin", RANDOM_BIN_MD5SUM,
                                          G_CHECKSUM_MD5, &error));
    g_assert_no_error(error);
}

static void
checksum_bad(void)
{
    g_autoptr(GError) error = NULL;

    g_assert_false(pu_checksum_verify_file("data/random.bin", "",
                                           G_CHECKSUM_SHA256, &error));
    g_assert_error(error, PU_ERROR, PU_ERROR_CHECKSUM);
    g_clear_error(&error);

    g_assert_false(pu_checksum_verify_file("data/random.bin", "",
                                           G_CHECKSUM_MD5, &error));
    g_assert_error(error, PU_ERROR, PU_ERROR_CHECKSUM);
    g_clear_error(&error);

    g_assert_false(pu_checksum_verify_file("file/not/found", "",
                                           G_CHECKSUM_MD5, &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
    g_clear_error(&error);
}

int
main(int argc,
     char *argv[])
{
    g_test_init(&argc, &argv, NULL);

#ifdef PARTUP_TEST_SRCDIR
    g_chdir(PARTUP_TEST_SRCDIR);
#endif

    g_test_add_func("/checksum/good", checksum_good);
    g_test_add_func("/checksum/bad", checksum_bad);

    return g_test_run();
}
