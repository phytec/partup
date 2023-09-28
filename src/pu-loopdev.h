/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_LOOPDEV_H
#define PARTUP_LOOPDEV_H

#define PU_LOOPDEV_ERROR (pu_loopdev_error_quark())

GQuark pu_loopdev_error_quark(void);

typedef enum {
    PU_LOOPDEV_ERROR_FAILED
} PutLoopdevError;

typedef struct {
    GFile *file;
    gchar *device;
} PuLoopdev;

PuLoopdev * pu_loopdev_new(gsize size,
                           GError **error);
gboolean pu_loopdev_attach(PuLoopdev *loopdev,
                           GError **error);
gboolean pu_loopdev_detach(PuLoopdev *loopdev,
                           GError **error);
void pu_loopdev_free(PuLoopdev *loopdev);

#endif /* PARTUP_LOOPDEV_H */
