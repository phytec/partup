Frequently Asked Questions
==========================

e2fsprogs: Filesystem has unsupported features
----------------------------------------------

When installing a partup package, the following error message may occur::

   10:25:12.608652 CRITICAL partup: Failed writing data to device: Failed resizing filesystem on '/dev/mmcblk0p2': Command 'resize2fs /dev/mmcblk0p2' failed with error message: 'resize2fs 1.46.5 (30-Dec-2021)
   resize2fs: Filesystem has unsupported feature(s) (/dev/mmcblk0p2)
   ': Child process exited with code 1

This happens because the host system executing the ``partup install`` command
uses e2fsprogs version 1.46 or older, not supporting the EXT4 ``orphan_file``
filesystem feature. The partup package to be installed contains an EXT4 image
with the ``orphan_file`` filesystem feature activated. But the host e2fsprogs
version is too old to understand this filesystem feature and consequently exits
with an error.

A solution to this issue is to update the host version of e2fsprogs to 1.47.0 or
newer. If this is not possible, a workaround would be to disable the
``orphan_file`` filesystem feature of the EXT4 image to be written.

In case e2fsprogs cannot be upgraded to 1.47.0 or newer, possible workarounds
are:

-  TODO
