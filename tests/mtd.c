/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include "pu-error.h"
#include "pu-mtd.h"

/* The device path is only needed to construct the object. Parsing a layout
 * configuration never accesses the device itself, so no real MTD device is
 * ever touched by these tests. */
static const gchar *device_path = "/dev/mtd0";

static void
mtd_ubi(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(PuConfig) config = NULL;
    g_autoptr(PuMtd) mtd = NULL;

    config = pu_config_new_from_file("config/ubi.yaml", &error);
    g_assert_no_error(error);
    g_assert_nonnull(config);

    mtd = pu_mtd_new(device_path, config, "data", FALSE, &error);
    g_assert_no_error(error);
    g_assert_nonnull(mtd);
}

static void
mtd_ubi_fail(const gchar *config_file,
             gint expected_code)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(PuConfig) config = NULL;
    g_autoptr(PuMtd) mtd = NULL;

    config = pu_config_new_from_file(config_file, &error);
    g_assert_no_error(error);
    g_assert_nonnull(config);

    mtd = pu_mtd_new(device_path, config, "data", FALSE, &error);
    g_assert_error(error, PU_ERROR, expected_code);
    g_assert_null(mtd);

    g_clear_error(&error);
}

static void
mtd_ubi_missing_name(void)
{
    mtd_ubi_fail("config/ubi-missing-name.yaml", PU_ERROR_UBI_PARSE);
}

static void
mtd_ubi_missing_size(void)
{
    mtd_ubi_fail("config/ubi-missing-size.yaml", PU_ERROR_UBI_PARSE);
}

static void
mtd_ubi_missing_volumes(void)
{
    mtd_ubi_fail("config/ubi-missing-volumes.yaml", PU_ERROR_UBI_PARSE);
}

static void
mtd_ubi_expand_not_last(void)
{
    mtd_ubi_fail("config/ubi-expand-not-last.yaml", PU_ERROR_UBI_PARSE);
}

static void
mtd_ubi_unknown_type(void)
{
    mtd_ubi_fail("config/ubi-unknown-type.yaml", PU_ERROR_UBI_PARSE);
}

static void
mtd_ubi_unknown_filesystem(void)
{
    mtd_ubi_fail("config/ubi-unknown-filesystem.yaml", PU_ERROR_UBI_PARSE);
}

static void
mtd_ubi_input_too_big(void)
{
    mtd_ubi_fail("config/ubi-input-too-big.yaml", PU_ERROR_UBI_PARSE);
}

static void
mtd_ubi_static_filesystem(void)
{
    mtd_ubi_fail("config/ubi-static-filesystem.yaml", PU_ERROR_UBI_PARSE);
}

static void
mtd_ubi_static_expand(void)
{
    mtd_ubi_fail("config/ubi-static-expand.yaml", PU_ERROR_UBI_PARSE);
}

static void
mtd_ubi_input_and_partition(void)
{
    mtd_ubi_fail("config/ubi-input-and-partition.yaml", PU_ERROR_MTD_PARSE);
}

int
main(int argc,
     char *argv[])
{
    g_test_init(&argc, &argv, NULL);

#ifdef PARTUP_TEST_SRCDIR
    g_chdir(PARTUP_TEST_SRCDIR);
#endif

    g_test_add_func("/mtd/ubi", mtd_ubi);
    g_test_add_func("/mtd/ubi/missing-name", mtd_ubi_missing_name);
    g_test_add_func("/mtd/ubi/missing-size", mtd_ubi_missing_size);
    g_test_add_func("/mtd/ubi/missing-volumes", mtd_ubi_missing_volumes);
    g_test_add_func("/mtd/ubi/expand-not-last", mtd_ubi_expand_not_last);
    g_test_add_func("/mtd/ubi/unknown-type", mtd_ubi_unknown_type);
    g_test_add_func("/mtd/ubi/unknown-filesystem", mtd_ubi_unknown_filesystem);
    g_test_add_func("/mtd/ubi/input-too-big", mtd_ubi_input_too_big);
    g_test_add_func("/mtd/ubi/static-filesystem", mtd_ubi_static_filesystem);
    g_test_add_func("/mtd/ubi/static-expand", mtd_ubi_static_expand);
    g_test_add_func("/mtd/ubi/input-and-partition", mtd_ubi_input_and_partition);

    return g_test_run();
}
