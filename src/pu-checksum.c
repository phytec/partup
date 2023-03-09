/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2022 PHYTEC Messtechnik GmbH
 */

#include <gio/gio.h>
#include "pu-checksum.h"
#include "pu-error.h"

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
    g_autoptr(GChecksum) checksum_obj = NULL;

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

    checksum_obj = g_checksum_new(checksum_type);
    g_checksum_update(checksum_obj, buffer, file_size);

    if (!g_str_equal(checksum, g_checksum_get_string(checksum_obj))) {
        g_set_error(error, PU_ERROR, PU_ERROR_CHECKSUM,
                    "Given checksum '%s' of file '%s' does not match",
                    checksum, filename);
        return FALSE;
    }

    return TRUE;
}
