/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2024 PHYTEC Messtechnik GmbH
 */

#define G_LOG_DOMAIN "partup-file"

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

gboolean
pu_file_read_int64(const gchar *filename,
                   gint64 *out,
                   GError **error)
{
    g_autofree gchar *content = NULL;
    gchar *endptr;

    if (!g_file_get_contents(filename, &content, NULL, error))
        return FALSE;

    *out = g_ascii_strtoll(content, &endptr, 10);
    if (*out == 0 && content == endptr) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Failed converting content of file '%s' to int64",
                    filename);
        return FALSE;
    }

    return TRUE;
}

gboolean
pu_file_copy(const gchar *src,
             const gchar *dest,
             GError **error)
{
    g_autoptr(GFile) in = NULL;
    g_autofree gchar *out_path = NULL;
    g_autoptr(GFile) out = NULL;

    g_return_val_if_fail(src != NULL, FALSE);
    g_return_val_if_fail(dest != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_debug("Copying '%s' to '%s'", src, dest);

    in = g_file_new_for_path(src);
    out_path = g_build_filename(dest, g_path_get_basename(src), NULL);
    out = g_file_new_for_path(out_path);

    return g_file_copy(in, out, G_FILE_COPY_NONE, NULL, NULL, NULL, error);
}

goffset
pu_file_get_size(const gchar *path,
                 GError **error)
{
    g_autoptr(GFile) file = NULL;
    g_autoptr(GFileInfo) file_info = NULL;

    g_return_val_if_fail(g_strcmp0(path, "") > 0, 0);
    g_return_val_if_fail(error == NULL || *error == NULL, 0);

    file = g_file_new_for_path(path);
    file_info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                  G_FILE_QUERY_INFO_NONE, NULL, error);
    if (file_info == NULL)
        return 0;

    return g_file_info_get_size(file_info);
}
