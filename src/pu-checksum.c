/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <gio/gio.h>
#include "pu-checksum.h"
#include "pu-error.h"
#include "pu-file.h"

gboolean
pu_checksum_verify_file(const gchar *filename,
                        const gchar *checksum,
                        GChecksumType checksum_type,
                        GError **error)
{
    g_autoptr(GFile) file = g_file_new_for_path(filename);
    g_autoptr(GFileInputStream) stream = NULL;
    g_autoptr(GFileInfo) file_info = NULL;
    goffset file_size;
    g_autofree guchar *buffer = NULL;
    g_autofree gchar *computed_checksum = NULL;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    stream = g_file_read(file, NULL, error);
    if (stream == NULL)
        return FALSE;

    file_info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                  G_FILE_QUERY_INFO_NONE, NULL, error);
    if (file_info == NULL)
        return FALSE;

    file_size = g_file_info_get_size(file_info);
    buffer = g_new0(guchar, file_size);
    if (!g_input_stream_read_all(G_INPUT_STREAM(stream), buffer, file_size,
                                 NULL, NULL, error))
        return FALSE;

    computed_checksum = g_compute_checksum_for_data(checksum_type, buffer, file_size);
    if (!g_str_equal(checksum, computed_checksum)) {
        g_set_error(error, PU_ERROR, PU_ERROR_CHECKSUM,
                    "Given checksum '%s' of file '%s' does not match '%s'",
                    checksum, filename, computed_checksum);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_checksum_verify_raw(const gchar *filename,
                       goffset offset,
                       gsize size,
                       const gchar *checksum,
                       GChecksumType checksum_type,
                       GError **error)
{
    g_autofree guchar *buffer = NULL;
    g_autofree gchar *computed_checksum = NULL;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!pu_file_read_raw(filename, &buffer, offset, size, NULL, error))
        return FALSE;

    computed_checksum = g_compute_checksum_for_data(checksum_type, buffer, size);
    if (!g_str_equal(checksum, computed_checksum)) {
        g_set_error(error, PU_ERROR, PU_ERROR_CHECKSUM,
                    "Given checksum '%s' of '%s' at offset %ld and size %ld does not match '%s'",
                    checksum, filename, offset, size, computed_checksum);
        return FALSE;
    }

    return TRUE;
}

gchar *
pu_checksum_new_from_file(const gchar *filename,
                          goffset offset,
                          GChecksumType checksum_type,
                          GError **error)
{
    g_autofree guchar *buffer = NULL;
    gsize bytes_read = 0;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!pu_file_read_raw(filename, &buffer, offset, -1, &bytes_read, error))
        return FALSE;

    return g_compute_checksum_for_data(checksum_type, buffer, bytes_read);
}
