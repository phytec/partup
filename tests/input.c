/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <pu-input.h>

#define LOREM_TXT_SHA256SUM   "25623b53e0984428da972f4c635706d32d01ec92dcd2ab39066082e0b9488c9d"
#define LOREM_TXT_MD5SUM      "3bc34a45d26784b5bea8529db533ae84"
#define LOREM_TXT_FILENAME    "lorem.txt"
#define LOREM_TAR_FILENAME    "lorem.tar"
#define DATA_DIR              "data"
#define LOREM_TXT_PATH        DATA_DIR "/" LOREM_TXT_FILENAME
#define LOREM_TAR_PATH        DATA_DIR "/" LOREM_TAR_FILENAME
#define LOREM_TXT_SIZE        12

static void
test_prefix_filename(void)
{
    g_autoptr(GError) error = NULL;
    PuInput input;
    input.filename = g_strdup(LOREM_TXT_FILENAME);
    g_assert_true(pu_input_prefix_filename(&input, DATA_DIR, &error));
    g_assert_no_error(error);
    g_assert_cmpstr(input.filename, ==, LOREM_TXT_PATH);
}

static void
test_validate_file(void)
{
    g_autoptr(GError) error = NULL;
    PuInput input;
    input.filename = g_strdup(LOREM_TXT_PATH);
    input.sha256sum = LOREM_TXT_SHA256SUM;
    input.md5sum = LOREM_TXT_MD5SUM;

    g_assert_true(pu_input_validate_file(&input, &error));
    g_assert_no_error(error);
}

static void
test_validate_file_no_checksum(void)
{
    g_autoptr(GError) error = NULL;
    PuInput input;
    input.filename = g_strdup(LOREM_TXT_PATH);
    input.sha256sum = "";
    input.md5sum = "";

    g_assert_false(pu_input_validate_file(&input, &error));
    g_assert_error(error, PU_INPUT_ERROR, PU_INPUT_ERROR_NO_CHECKSUM);
}

static void
test_input_get_size(void)
{
    g_autoptr(GError) error = NULL;
    PuInput input;
    input.filename = g_strdup(LOREM_TXT_PATH);
    input.sha256sum = "";
    input.md5sum = "";

    g_assert_true(pu_input_get_size(&input, &error));
    g_assert_no_error(error);
    g_assert_cmpuint(input._size, ==, LOREM_TXT_SIZE);
}

static void
test_input_get_size_tar(void)
{
    g_autoptr(GError) error = NULL;
    PuInput input;
    input.filename = g_strdup(LOREM_TAR_PATH);
    input.sha256sum = "";
    input.md5sum = "";

    g_assert_true(pu_input_get_size(&input, &error));
    g_assert_no_error(error);
    g_assert_cmpuint(input._size, ==, LOREM_TXT_SIZE);
}

int
main(int argc,
     char *argv[])
{
    g_test_init(&argc, &argv, NULL);

#ifdef PARTUP_TEST_SRCDIR
    g_chdir(PARTUP_TEST_SRCDIR);
#endif

    g_test_add_func("/input/prefix_filename", test_prefix_filename);
    g_test_add_func("/input/validate_file", test_validate_file);
    g_test_add_func("/input/validate_file_no_checksum", test_validate_file_no_checksum);
    g_test_add_func("/input/input_get_size", test_input_get_size);
    g_test_add_func("/input/input_get_size_tar", test_input_get_size_tar);

    return g_test_run();
}
