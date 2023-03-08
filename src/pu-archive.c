/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "pu-archive.h"
#include "pu-error.h"
#include "pu-utils.h"

static gint
pu_archive_copy_data(struct archive *read,
                     struct archive *write)
{
    gint res;
    gconstpointer buf;
    gsize size;
    goffset offset;

    while (TRUE) {
        res = archive_read_data_block(read, &buf, &size, &offset);
        if (res == ARCHIVE_EOF)
            return ARCHIVE_OK;
        if (res < ARCHIVE_OK)
            return res;

        res = archive_write_data_block(write, buf, size, offset);
        if (res < ARCHIVE_OK)
            return res;
    }
}

gboolean
pu_archive_extract(const gchar *filename,
                   const gchar *dest,
                   GError **error)
{

    struct archive *ar;
    struct archive *ext;
    struct archive_entry *entry;
    g_autofree gchar *old_dest = NULL;
    gint flags;
    gint read;
    gint write;

    g_return_val_if_fail(filename != NULL, FALSE);
    g_return_val_if_fail(dest != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    flags = ARCHIVE_EXTRACT_TIME |
            ARCHIVE_EXTRACT_PERM |
            ARCHIVE_EXTRACT_OWNER |
            ARCHIVE_EXTRACT_ACL |
            ARCHIVE_EXTRACT_FFLAGS;

    ar = archive_read_new();
    archive_read_support_filter_all(ar);
    archive_read_support_format_all(ar);
    ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, flags);
    archive_write_disk_set_standard_lookup(ext);
    read = archive_read_open_filename(ar, filename, 10240);

    if (read != ARCHIVE_OK) {
        g_set_error(error, PU_ERROR, PU_ERROR_ARCHIVE_READ,
                    "Failed reading archive '%s'", filename);
        return FALSE;
    }

    old_dest = g_get_current_dir();
    if (!g_chdir(dest)) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Failed changing directory before extracting");
        archive_read_close(ar);
        archive_read_free(ar);
        archive_write_close(ext);
        archive_write_free(ext);
        return FALSE;
    }

    while (TRUE) {
        read = archive_read_next_header(ar, &entry);
        if (read == ARCHIVE_EOF)
            break;
        if (read < ARCHIVE_WARN) {
            g_set_error(error, PU_ERROR, PU_ERROR_ARCHIVE_READ,
                        "Failed reading header: %s", archive_error_string(ar));
            return FALSE;
        }

        write = archive_write_header(ext, entry);
        if (write < ARCHIVE_WARN) {
            g_set_error(error, PU_ERROR, PU_ERROR_ARCHIVE_WRITE,
                        "Failed writing header: %s", archive_error_string(ext));
            return FALSE;
        }
        if (archive_entry_size(entry) > 0) {
            write = pu_archive_copy_data(ar, ext);
            if (write < ARCHIVE_WARN) {
                g_set_error(error, PU_ERROR, PU_ERROR_ARCHIVE_WRITE,
                            "Failed copying data: %s", archive_error_string(ext));
                return FALSE;
            }
        }
        write = archive_write_finish_entry(ext);
        if (write < ARCHIVE_WARN) {
            g_set_error(error, PU_ERROR, PU_ERROR_ARCHIVE_WRITE,
                        "Failed finishing write: %s", archive_error_string(ext));
            return FALSE;
        }
    }

    archive_read_close(ar);
    archive_read_free(ar);
    archive_write_close(ext);
    archive_write_free(ext);
    g_chdir(old_dest);

    return TRUE;
}
