partup Documentation
====================

Introduction
------------

partup is a program used for initializing flash memory on a target device using
a predefined layout.

Main Features
-------------

-  Partitioning block devices
-  Automatic partition size calculation to use whole device
-  Supported disklabels: ``gpt``, ``msdos``
-  Supported filesystems: ``fat16``, ``fat32``, ``ext2``, ``ext3``, ``ext4``
-  Support for eMMC boot partitions
-  Write files and extract archives to partitions
-  Write raw binaries to device at specified offset
-  Checksum verification for input files (MD5, SHA256)
-  Set PARTUUID and partition flags

.. toctree::
   :maxdepth: 2
   :caption: Contents

   usage
   layout-config
   package
   contributing
   changelog
