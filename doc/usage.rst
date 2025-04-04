Usage
=====

General Usage
-------------

partup has different commands for executing all needed steps during the initial
flashing process of a target device. The process is usually as follows:

1. Create a partup package containing the layout configuration and any required
   input files.
2. Install this partup package to the device.

partup also provides a command for displaying the contents of a partup package.
For a detailed description on how these packages work and how to create them,
see :ref:`example-usage`.

The general usage syntax looks like the following::

   partup [OPTION…] COMMAND ARGUMENTS

Global Option List
------------------

When executing partup, the following options can be specified independently of
any command:

-h, --help                          Show help options
-d, --debug                         Print debug messages
-D, --debug-domains=DEBUG_DOMAINS   Comma separated list of modules to enable
                                    debug output for
-q, --quiet                         Only print error messages

Commands
--------

install [OPTION…] *PACKAGE* *DEVICE*
   Install a partup PACKAGE to DEVICE

   -s, --skip-checksums    Skip checksum verification for all input files

package [OPTION…] *PACKAGE* *FILES…*
   Create a partup PACKAGE with the contents FILES

   -C, --directory=DIR     Change to DIR before creating the package
   -f, --force             Overwrite any existing package

show [OPTION…] *PACKAGE*
   List the contents of a partup PACKAGE

   -s, --size              Print the size of each file

version
   Print the program version

Supported Output Devices
------------------------

Writing is currently supported for block devices and those incorporating a flash
translation layer. This includes:

-  HDD
-  SSD
-  SD cards
-  eMMC devices and their eMMC boot partitions

The device must be named ``mmcblk*`` or ``sd*``, e.g. the following device names
are valid::

   /dev/mmcblk0
   /dev/mmcblk9
   /dev/sda
   /dev/sdf

.. warning::

   Do *not* attempt to write to existing partitions, like ``/dev/mmcblk1p2``!
   Specify the raw device, as mentioned above, instead.

.. note::

   Devices with raw access to memory, not incorporating a flash translation
   layer, like those accessible through the `Linux MTD interface
   <http://www.linux-mtd.infradead.org/>`_, are currently not supported.

.. _example-usage:

Example Usage
-------------

Creating partup Packages
........................

partup packages use the `SquashFS filesystem
<https://github.com/plougher/squashfs-tools>`__ to provide a read-only image
containing all required input files and the :doc:`layout configuration file
<layout-config-reference>`. The layout configuration file must be the only
``.yaml`` file and be placed at the root of the package. When using partup's
builtin command ``package`` to create one, these requirements are automatically
checked against.

Creating a package is as easy as specifying an output filename for the package,
its input files and the layout configuration file as the only ``.yaml`` file::

   partup package mypackage.partup u-boot.bin zImage rootfs.tar.gz layout.yaml

.. note::

   The first filename provided after the ``package`` command is always the
   output filename of the package.

.. note::

   The extension of a partup package should be named ``.partup``, although this
   is just a recommendation for easier distinction from other file types and is
   not strictly needed.

Viewing partup Package Contents
...............................

The content of a package can be listed using the ``show`` command::

   partup show mypackage.partup

   u-boot.bin
   zImage
   rootfs.tar.gz
   layout.yaml


Installing partup Packages
..........................

A partup package contains all the information needed to install the initial data
to a device. The ``install`` command then only needs the desired flash device to
be specified::

   partup install mypackage.partup /dev/mmcblk0
