Usage
=====

General Usage
-------------

partup requires a configuration file specifying the layout and an output device
to write to as its mandatory arguments. The full syntax is as follows::

   partup [OPTION…] -c CONFIG DEVICE

An minimal example may look like the following::

   partup -c layout.yaml /dev/mmcblk2

Global Option List
------------------

When executing partup, the following options can be specified independently for
all commands:

-h, --help              Show help options
-d, --debug             Print debug messages
-v, --version           Print the program version and exit

Commands
--------

install [OPTION…] *PACKAGE* *DEVICE*
   Install a partup PACKAGE to DEVICE

   -s, --skip-checksums    Skip checksum verification for all input files

package *PACKAGE* *FILES…*
   Create a partup PACKAGE with the contents FILES

show *PACKAGE*
   List the contents of a partup PACKAGE

version
   Print the program version

help
   Show help options

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

Devices with raw access to memory, not incorporating a flash translation layer,
like those accessible through the `Linux MTD interface
<http://www.linux-mtd.infradead.org/>`_, are currently not supported.
