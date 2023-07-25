/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_INPUT_H
#define PARTUP_INPUT_H

#define PU_INPUT_ERROR (pu_input_error_quark())

GQuark pu_input_error_quark(void);

typedef enum {
    PU_INPUT_ERROR_NO_CHECKSUM,
    PU_INPUT_ERROR_FILE_NOT_FOUND
} PuInputError;

typedef struct {
    gchar *filename;
    gchar *md5sum;
    gchar *sha256sum;

    /* Internal members */
    gsize _size;
} PuInput;

gboolean pu_input_validate_file(PuInput *input,
                                GError **error);
gboolean pu_input_prefix_filename(PuInput *input,
                                  const gchar *prefix,
                                  GError **error);
gboolean pu_input_get_size(PuInput *input,
                           GError **error);

#endif /* PARTUP_INPUT_H */
