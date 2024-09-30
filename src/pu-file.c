/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2024 PHYTEC Messtechnik GmbH
 */

#include <gio/gio.h>
#include "pu-error.h"
#include "pu-file.h"

gboolean
pu_file_read_raw(const gchar *filename,
                 guchar **buffer,
                 goffset offset,
                 gssize count,
                 gsize *bytes_read,
                 GError **error)
{
    g_autoptr(GFile) file = g_file_new_for_path(filename);
    g_autoptr(GFileInputStream) stream = NULL;
    g_autoptr(GFileInfo) file_info = NULL;
    goffset file_size;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    stream = g_file_read(file, NULL, error);
    if (stream == NULL)
        return FALSE;

    file_info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                  G_FILE_QUERY_INFO_NONE, NULL, error);
    if (file_info == NULL)
        return FALSE;

    file_size = g_file_info_get_size(file_info);
    if (count < 0)
        count = file_size - offset;

    if (g_input_stream_skip(G_INPUT_STREAM(stream), offset, NULL, error) < 0)
        return FALSE;

    *buffer = g_new0(guchar, count);
    if (!g_input_stream_read_all(G_INPUT_STREAM(stream), *buffer, count,
                                 bytes_read, NULL, error))
        return FALSE;

    return TRUE;
}
