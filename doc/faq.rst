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
