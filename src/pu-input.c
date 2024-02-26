/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#define G_LOG_DOMAIN "partup-input"

#include "pu-checksum.h"
#include "pu-glib-compat.h"
#include "pu-input.h"
#include "pu-utils.h"

G_DEFINE_QUARK(pu-input-context-error-quark, pu_input_error)

gboolean
pu_input_validate_file(PuInput *input,
                       GError **error)
{
    gboolean validated = FALSE;
    gchar *path;

    g_return_val_if_fail(input != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    path = input->filename;

    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        g_set_error(error, PU_INPUT_ERROR, PU_INPUT_ERROR_FILE_NOT_FOUND,
                    "Input file '%s' does not exist", path);
        return FALSE;
    }

    if (!g_str_equal(input->md5sum, "")) {
        g_debug("Checking MD5 sum of input file '%s'", path);
        if (!pu_checksum_verify_file(path, input->md5sum, G_CHECKSUM_MD5, error))
            return FALSE;
        validated = TRUE;
    }

    if (!g_str_equal(input->sha256sum, "")) {
        g_debug("Checking SHA256 sum of input file '%s'", path);
        if (!pu_checksum_verify_file(path, input->sha256sum, G_CHECKSUM_SHA256, error))
            return FALSE;
        validated = TRUE;
    }

    if (!validated) {
        g_set_error(error, PU_INPUT_ERROR, PU_INPUT_ERROR_NO_CHECKSUM,
                    "No checksum provided for '%s'", path);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_input_prefix_filename(PuInput *input,
                         const gchar *prefix,
                         GError **error)
{
    g_autofree gchar *path = NULL;

    g_return_val_if_fail(input != NULL, FALSE);
    g_return_val_if_fail(prefix != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    path = pu_path_from_filename(input->filename, prefix, error);
    if (path == NULL) {
        g_prefix_error(error, "Failed parsing input filename: ");
        return FALSE;
    }

    g_free(input->filename);
    input->filename = g_steal_pointer(&path);

    return TRUE;
}

gboolean
pu_input_get_size(PuInput *input,
                  GError **error)
{
    g_autofree gchar *cmd = NULL;
    g_autofree gchar *out = NULL;

    g_return_val_if_fail(input != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (g_regex_match_simple(".tar", input->filename, G_REGEX_CASELESS, 0)) {
        cmd = g_strdup_printf("sh -c 'tar -xf %s -O | wc -c'", input->filename);
        if (!pu_spawn_command_line_sync_result(cmd, &out, error))
            return FALSE;
        input->_size = (gsize) g_ascii_strtoull(out, NULL, 10);
    } else {
        input->_size = pu_get_file_size(input->filename, error);
        if (!input->_size)
            return FALSE;
    }

    return TRUE;
}
