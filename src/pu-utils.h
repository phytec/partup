/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2023 PHYTEC Messtechnik GmbH
 */

#ifndef PARTUP_UTILS_H
#define PARTUP_UTILS_H

#include <glib.h>
#include <parted/parted.h>

gboolean pu_spawn_command_line_sync_result(const gchar *command_line,
                                           gchar **result,
                                           GError **error);
gboolean pu_spawn_command_line_sync(const gchar *command_line,
                                    GError **error);
gboolean pu_file_copy(const gchar *src,
                      const gchar *dest,
                      GError **error);
gboolean pu_archive_extract(const gchar *filename,
                            const gchar *dest,
                            GError **error);
gboolean pu_make_filesystem(const gchar *part,
                            const gchar *type,
                            const gchar *label,
                            const gchar *extra_args,
                            GError **error);
gboolean pu_set_ext_label(const gchar *part,
                          const gchar *label,
                          GError **error);
gboolean pu_resize_filesystem(const gchar *part,
                              GError **error);
gboolean pu_write_raw(const gchar *input_path,
                      const gchar *output_path,
                      PedDevice *device,
                      PedSector input_offset,
                      PedSector output_offset,
                      PedSector size,
                      GError **error);
gboolean pu_has_bootpart(const gchar *device);
gboolean pu_write_raw_bootpart(const gchar *input,
                               PedDevice *device,
                               guint bootpart,
                               PedSector input_offset,
                               PedSector output_offset,
                               GError **error);
gboolean pu_bootpart_enable(const gchar *device,
                            guint bootpart,
                            gboolean boot_ack,
                            GError **error);
gboolean pu_partition_set_partuuid(const gchar *device,
                                   guint index,
                                   const gchar *partuuid,
                                   GError **error);
gboolean pu_is_drive(const gchar *device);
gboolean pu_wait_for_partitions(GError **error);
goffset pu_get_file_size(const gchar *path,
                         GError **error);
gchar * pu_path_from_filename(const gchar *filename,
                              const gchar *prefix,
                              GError **error);
gchar * pu_device_get_partition_path(const gchar *device,
                                     guint index,
                                     GError **error);
gchar * pu_device_get_partition_pattern(const gchar *device,
                                        GError **error);
gchar * pu_str_pre_remove(gchar *string,
                          guint n);

#endif /* PARTUP_UTILS_H */
