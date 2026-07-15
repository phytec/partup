/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 PHYTEC Messtechnik GmbH
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <parted/parted.h>
#include "helper.h"
#include "pu-emmc.h"
#include "pu-error.h"
#include "pu-utils.h"

static gboolean
check_partition_alignment(PedDevice *dev,
                          PedAlignment *alignment)
{
    PedDisk *disk;
    PedPartition *part;

    disk = ped_disk_new(dev);
    if (disk == NULL) {
        return FALSE;
    }

    for (part = ped_disk_next_partition(disk, NULL); part; part = ped_disk_next_partition(disk, part)) {
        if (!ped_partition_is_active(part)) {
            continue;
        }

        /* Partition start and length must be divisible by the chosen alignment
         * grain size. Otherwise, the partition is not aligned properly. */
        if (part->geom.start % alignment->grain_size) {
            return FALSE;
        }
        if (part->geom.length % alignment->grain_size) {
            return FALSE;
        }
    }

    ped_disk_destroy(disk);
    return TRUE;
}

static gboolean
check_partition_fstype(PedDevice *dev,
                       int part_idx,
                       const gchar *name)
{
    PedDisk *disk;
    PedPartition *part;
    gboolean is_correct_fstype = FALSE;

    disk = ped_disk_new(dev);
    if (disk == NULL) {
        return FALSE;
    }

    part = ped_disk_get_partition(disk, part_idx);
    if (part == NULL) {
        return FALSE;
    }

    if (!part->fs_type) {
        if (!name) {
            is_correct_fstype = TRUE;
        }
    } else if (g_str_equal(part->fs_type->name, name)) {
        is_correct_fstype = TRUE;
    }

    ped_disk_destroy(disk);
    return is_correct_fstype;
}

static void
test_emmc_align_minimal(EmptyDeviceFixture *fixture,
                        G_GNUC_UNUSED gconstpointer user_data)
{
    g_autoptr(PuConfig) config = NULL;
    g_autoptr(PuEmmc) emmc = NULL;
    PedDevice *dev = NULL;

    config = pu_config_new_from_file("config/system-tests/align-minimal.yaml",
                                     &fixture->error);
    g_assert_nonnull(config);

    emmc = pu_emmc_new(fixture->loop_dev, config, "data", FALSE, &fixture->error);
    g_assert_nonnull(emmc);

    g_assert_true(pu_flash_init_device(PU_FLASH(emmc), &fixture->error));
    g_assert_true(pu_flash_setup_layout(PU_FLASH(emmc), &fixture->error));

    dev = ped_device_get(fixture->loop_dev);
    g_assert_nonnull(dev);
    g_assert_true(check_partition_alignment(dev, pu_emmc_get_alignment(emmc)));
}

static void
test_emmc_align_optimal(EmptyDeviceFixture *fixture,
                        G_GNUC_UNUSED gconstpointer user_data)
{
    g_autoptr(PuConfig) config = NULL;
    g_autoptr(PuEmmc) emmc = NULL;
    PedDevice *dev = NULL;

    config = pu_config_new_from_file("config/system-tests/align-optimal.yaml",
                                     &fixture->error);
    g_assert_nonnull(config);

    emmc = pu_emmc_new(fixture->loop_dev, config, "data", FALSE, &fixture->error);
    g_assert_nonnull(emmc);

    g_assert_true(pu_flash_init_device(PU_FLASH(emmc), &fixture->error));
    g_assert_true(pu_flash_setup_layout(PU_FLASH(emmc), &fixture->error));

    dev = ped_device_get(fixture->loop_dev);
    g_assert_nonnull(dev);
    g_assert_true(check_partition_alignment(dev, pu_emmc_get_alignment(emmc)));
}

static void
test_partition_filesystem(EmptyDeviceFixture *fixture,
                          G_GNUC_UNUSED gconstpointer user_data)
{
    g_autoptr(PuConfig) config = NULL;
    g_autoptr(PuEmmc) emmc = NULL;
    PedDevice *dev = NULL;

    config = pu_config_new_from_file("config/gpt-partition-filesystem.yaml",
                                     &fixture->error);
    g_assert_nonnull(config);

    emmc = pu_emmc_new(fixture->loop_dev, config, "data", FALSE, &fixture->error);
    g_assert_nonnull(emmc);

    g_assert_true(pu_flash_init_device(PU_FLASH(emmc), &fixture->error));
    g_assert_true(pu_flash_setup_layout(PU_FLASH(emmc), &fixture->error));
    g_assert_true(pu_flash_write_data(PU_FLASH(emmc), &fixture->error));

    dev = ped_device_get(fixture->loop_dev);
    g_assert_nonnull(dev);
    /* The default for "filesystem" should be "null", as per the documentation */
    g_assert_true(check_partition_fstype(dev, 1, NULL));
    g_assert_true(check_partition_fstype(dev, 2, NULL));
    g_assert_true(check_partition_fstype(dev, 3, "fat16"));
    g_assert_true(check_partition_fstype(dev, 4, "fat32"));
    g_assert_true(check_partition_fstype(dev, 5, "ext2"));
    g_assert_true(check_partition_fstype(dev, 6, "ext3"));
    g_assert_true(check_partition_fstype(dev, 7, "ext4"));
}

int
main(int argc,
     char *argv[])
{
    /* Skip tests when not run as root */
    if (getuid() != 0)
        return 77;

    g_test_init(&argc, &argv, NULL);

#ifdef PARTUP_TEST_SRCDIR
    g_chdir(PARTUP_TEST_SRCDIR);
#endif

    g_test_add("/emmc/align_minimal", EmptyDeviceFixture, NULL, empty_device_set_up,
               test_emmc_align_minimal, empty_device_tear_down);
    g_test_add("/emmc/align_optimal", EmptyDeviceFixture, NULL, empty_device_set_up,
               test_emmc_align_optimal, empty_device_tear_down);
    g_test_add("/emmc/partition_filesystem", EmptyDeviceFixture, NULL,
               empty_device_set_up, test_partition_filesystem,
               empty_device_tear_down);

    return g_test_run();
}
