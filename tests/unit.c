/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2025 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "pu-unit.h"
#include "pu-error.h"

static void
unit_factor(void)
{
    g_assert_cmpint(pu_unit_get_factor("kB"), ==, 1000LL);
    g_assert_cmpint(pu_unit_get_factor("MB"), ==, 1000LL * 1000LL);
    g_assert_cmpint(pu_unit_get_factor("kiB"), ==, 1024LL);
    g_assert_cmpint(pu_unit_get_factor("MiB"), ==, 1024LL * 1024LL);
    g_assert_cmpint(pu_unit_get_factor("gib"), ==, 1024LL * 1024LL * 1024LL);
    g_assert_cmpint(pu_unit_get_factor("tb"), ==, 1000LL * 1000LL * 1000LL * 1000LL);
    g_assert_cmpint(pu_unit_get_factor("TIB"), ==, 1024LL * 1024LL * 1024LL * 1024LL);

    g_assert_cmpint(pu_unit_get_factor("bad"), <, 0);
    g_assert_cmpint(pu_unit_get_factor("wrong"), <, 0);
}

static void
unit_parse_bytes(void)
{
    gint64 out;

    g_assert_true(pu_unit_parse_bytes("0", &out));
    g_assert_cmpint(out, ==, 0);
    g_assert_true(pu_unit_parse_bytes("1", &out));
    g_assert_cmpint(out, ==, 1);
    g_assert_true(pu_unit_parse_bytes("78402238", &out));
    g_assert_cmpint(out, ==, 78402238LL);

    g_assert_true(pu_unit_parse_bytes("7B", &out));
    g_assert_cmpint(out, ==, 7LL);
    g_assert_true(pu_unit_parse_bytes("32kiB", &out));
    g_assert_cmpint(out, ==, 32LL * 1024LL);
    g_assert_true(pu_unit_parse_bytes("12MiB", &out));
    g_assert_cmpint(out, ==, 12LL * 1024LL * 1024LL);
    g_assert_true(pu_unit_parse_bytes("9gib", &out));
    g_assert_cmpint(out, ==, 9LL * 1024LL * 1024LL * 1024LL);
    g_assert_true(pu_unit_parse_bytes("42kB", &out));
    g_assert_cmpint(out, ==, 42LL * 1000LL);
    g_assert_true(pu_unit_parse_bytes("560TB", &out));
    g_assert_cmpint(out, ==, 560LL * 1000LL * 1000LL * 1000LL * 1000LL);

    g_assert_false(pu_unit_parse_bytes("-1", &out));
    g_assert_false(pu_unit_parse_bytes("-8.56", &out));
    g_assert_false(pu_unit_parse_bytes("2.1", &out));
    g_assert_false(pu_unit_parse_bytes("3,406", &out));
}

int
main(int argc,
     char *argv[])
{
    g_test_init(&argc, &argv, NULL);

#ifdef PARTUP_TEST_SRCDIR
    g_chdir(PARTUP_TEST_SRCDIR);
#endif

    g_test_add_func("/unit/factor", unit_factor);
    g_test_add_func("/unit/parse_bytes", unit_parse_bytes);

    return g_test_run();
}
