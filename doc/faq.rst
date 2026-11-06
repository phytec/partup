.. _faq:

Frequently Asked Questions
==========================

.. _faq-unsupported-features:

e2fsprogs: Filesystem has unsupported features
----------------------------------------------

When installing a partup package, the following error message may occur::

   Failed resizing filesystem on '/dev/mmcblk0p2': Command 'resize2fs
   /dev/mmcblk0p2' failed with error message: 'resize2fs 1.46.5 (30-Dec-2021)
   resize2fs: Filesystem has unsupported feature(s) (/dev/mmcblk0p2)

This happens because the host system executing the ``partup install`` command
uses e2fsprogs version 1.46 or older, which does not support the ext4
``orphan_file`` filesystem feature. The partup package to be installed contains
an ext4 image with the ``orphan_file`` feature enabled. Since the host
e2fsprogs version is too old to understand this feature, ``resize2fs`` exits
with an error.

The ``orphan_file`` feature was introduced with e2fsprogs 1.47.0. Starting with
that version, ``mke2fs`` enables it by default for new ext4 filesystems.

Possible solutions, in order of preference, are:

1. Update e2fsprogs on the host executing ``partup install`` to version 1.47.0
   or newer.

2. Create the ext4 image without the ``orphan_file`` feature, e.g. by passing
   ``-O ^orphan_file`` to ``mkfs.ext4``. When building images with a build
   system, pass this option to the tool creating the ext4 image, or use a
   custom ``mke2fs.conf`` without ``orphan_file`` in the ``features`` list of
   the ext4 filesystem type. The ``MKE2FS_CONFIG`` environment variable selects
   the configuration file to use.

3. Disable the feature on an already existing image using a machine with
   e2fsprogs 1.47.0 or newer, and create a new partup package with the
   modified image:

   .. code-block:: sh

      e2fsck -f image.ext4
      tune2fs -O ^orphan_file image.ext4

.. note::

   Linux kernels older than 6.5 do not support the ``orphan_file`` feature and
   can only mount such filesystems read-only. Keep this in mind when choosing
   the kernel version of the target system.

.. _faq-partup-vs-wic:

What is the difference between partup and WKS/WIC?
--------------------------------------------------

partup was developed because some features were missing in other partitioning
and image tools. Most of them are conceptually out of scope for WIC and cannot
easily be added there:

-  **Dynamic partition sizes:** Most tools only allow fixed partition sizes or
   percentages, which require knowing the size of the whole disk. This size is
   usually not known when the image is built (e.g. when using Yocto) and can
   vary a lot between devices. partup can expand partitions to fill the
   remaining space of the device at install time (see ``expand`` in
   :doc:`layout-config-reference`).
-  **eMMC-specific features:** eMMC boot partitions (like
   ``/dev/mmcblk2boot0``) can be written as part of the same package, instead
   of in a separate step using additional tools like ``mmc``. Other eMMC
   features, like the boot bus and H/W reset configuration (``bootbus`` and
   ``hwreset``), are also configurable. Some of these are one-time
   programmable. WIC cannot handle these features.

Further motivations were:

-  **Write speed:** Writing WIC images can take a long time, especially with a
   root filesystem larger than a few hundred megabytes. bmaptool mostly solves
   this, but it skips holes (zero-valued bytes). Old data on the disk, like a
   saved U-Boot environment of a previous system, then stays and may be
   incompatible with the newly flashed system. partup can clear specific
   regions of the disk (see ``clean`` in :doc:`layout-config-reference`) and
   creates the partition table and filesystems anew.
-  **Comfortable creation and installation:** A package is created and
   installed with a single command each. See :ref:`example-usage`.
-  **Using the full disk space:** The same package can be installed to devices
   with different disk sizes while utilizing the whole space.
-  **Small image size:** WIC images store their content sequentially and raw,
   so they can be several hundred megabytes or even gigabytes large. They can
   be compressed, e.g. with XZ, but (de)compression takes a lot of time. partup
   packages use SquashFS, which is much smaller than an uncompressed image, is
   fast to read and can easily be mounted.
-  **Different flash types:** WIC images are meant for managed flash devices,
   like SD cards, eMMCs and SSDs. Raw NAND and NOR flash usually needs a
   different image format and individually flashed files. partup supports
   MTD devices since :ref:`release-3.0.0`.

partup can be used whenever an image is written to an SD card, eMMC or SSD of
an embedded device. It can be used for non-embedded devices as well, although
embedded devices are its main use case.
