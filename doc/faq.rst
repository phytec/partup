Frequently Asked Questions
==========================

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
e2fsprogs version is too old to understand this feature, `resize2fs`_ exits
with an error.

The ``orphan_file`` feature was introduced with `e2fsprogs 1.47.0`_. Starting
with that version, `mke2fs`_ enables it by default for new ext4 filesystems.

Possible solutions, in order of preference, are:

1. Update e2fsprogs on the host executing ``partup install`` to version
   `1.47.0 <e2fsprogs 1.47.0_>`_ or newer.

2. Create the ext4 image without the ``orphan_file`` feature, e.g. by passing
   ``-O ^orphan_file`` to `mkfs.ext4 <mke2fs_>`_. When building images with a
   build system, pass this option to the tool creating the ext4 image, or use a
   custom `mke2fs.conf`_ without ``orphan_file`` in the ``features`` list of
   the ext4 filesystem type. The ``MKE2FS_CONFIG`` environment variable selects
   the configuration file to use.

3. Disable the feature on an already existing image using a machine with
   e2fsprogs 1.47.0 or newer, and create a new partup package with the
   modified image:

   .. code-block:: sh

      e2fsck -f image.ext4
      tune2fs -O ^orphan_file image.ext4

   See the `e2fsck`_ and `tune2fs`_ man pages for details.

.. note::

   Linux kernels older than 6.5 do not support the ``orphan_file`` feature and
   can only mount such filesystems read-only. Keep this in mind when choosing
   the kernel version of the target system.

.. _e2fsprogs 1.47.0: https://e2fsprogs.sourceforge.net/e2fsprogs-release.html#1.47.0
.. _resize2fs: https://man7.org/linux/man-pages/man8/resize2fs.8.html
.. _mke2fs: https://man7.org/linux/man-pages/man8/mke2fs.8.html
.. _mke2fs.conf: https://man7.org/linux/man-pages/man5/mke2fs.conf.5.html
.. _e2fsck: https://man7.org/linux/man-pages/man8/e2fsck.8.html
.. _tune2fs: https://man7.org/linux/man-pages/man8/tune2fs.8.html
