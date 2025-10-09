/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2025 PHYTEC Messtechnik GmbH
 */

#include <fcntl.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <linux/mmc/ioctl.h>
#include <sys/ioctl.h>
#include "pu-emmc-utils.h"
#include "pu-error.h"
#include "pu-utils.h"

/* From kernel include/linux/mmc/mmc.h */
#define MMC_SEND_EXT_CSD    8

/* From kernel include/linux/mmc/core.h */
#define MMC_RSP_PRESENT     (1 << 0)
#define MMC_RSP_CRC         (1 << 2)
#define MMC_RSP_OPCODE      (1 << 4)
#define MMC_CMD_ADTC        (1 << 5)
#define MMC_RSP_R1          (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define MMC_RSP_SPI_S1      (1 << 7)
#define MMC_RSP_SPI_R1      (MMC_RSP_SPI_S1)

gboolean
pu_emmc_utils_read_extcsd(const gchar *device,
                          guint8 ext_csd[512],
                          GError **error)
{
    gint fd;
    gint ret = 0;
    struct mmc_ioc_cmd idata = {
        .write_flag = 0,
        .opcode = MMC_SEND_EXT_CSD,
        .arg = 0,
        .flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC,
        .blksz = 512,
        .blocks = 1
    };

    g_return_val_if_fail(g_strcmp0(device, "") > 0, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, 0);

    fd = g_open(device, O_RDONLY);
    if (fd < 0) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Failed opening device %s to read EXT_CSD", device);
        return FALSE;
    }

    memset(ext_csd, 0, sizeof(guint8) * 512);
    mmc_ioc_cmd_set_data(idata, ext_csd);

    ret = ioctl(fd, MMC_IOC_CMD, &idata);
    if (ret) {
        g_set_error(error, PU_ERROR, PU_ERROR_FAILED,
                    "Failed calling ioctl(MMC_IOC_CMD) command to %s", device);
        g_close(fd, NULL);
        return FALSE;
    }

    if (!g_close(fd, error))
        return FALSE;

    return TRUE;
}

gboolean
pu_emmc_utils_read_max_enh_area_size(const gchar *device,
                                     gint64 *max_size,
                                     GError **error)
{
    gint64 wp_size;
    gint64 erase_size;
    gint64 mult;
    guint8 ext_csd[512];

    g_return_val_if_fail(g_strcmp0(device, "") > 0, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, 0);

    if (!pu_emmc_utils_read_extcsd(device, ext_csd, error))
        return FALSE;

    wp_size = ext_csd[221];
    erase_size = ext_csd[224];
    mult = (ext_csd[159] << 16) | (ext_csd[158] << 8) | ext_csd[157];
    *max_size = 512l * mult * wp_size * erase_size;

    return TRUE;
}

gboolean
pu_emmc_utils_set_enh_area(const gchar *device,
                           const gchar *enh_area,
                           GError **error)
{
    g_autofree gchar *cmd = NULL;
    g_autofree gchar *opts = NULL;
    gint64 max_size;

    g_return_val_if_fail(g_strcmp0(device, "") > 0, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, 0);

    if (g_strcmp0(enh_area, "") <= 0)
        return TRUE;

    if (g_regex_match_simple("^(max|full|all|auto)$", enh_area, 0, 0)) {
        if (!pu_emmc_utils_read_max_enh_area_size(device, &max_size, error))
            return FALSE;

        opts = g_strdup_printf("0 %" G_GINT64_FORMAT, max_size);
    } else {
        opts = g_strdup(enh_area);
    }

    cmd = g_strdup_printf("mmc enh_area set -n %s %s", opts, device);

    if (!pu_spawn_command_line_sync(cmd, error))
        return FALSE;

    return TRUE;
}
