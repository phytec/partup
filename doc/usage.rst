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
see the page :doc:`package`.

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

package [OPTION…] *PACKAGE* *FILES…*
   Create a partup PACKAGE with the contents FILES

   -C, --directory=DIR     Change to DIR before creating the package
   -f, --force             Overwrite any existing package

show [OPTION…] *PACKAGE*
   List the contents of a partup PACKAGE

   -s, --size              Print the size of each file

validate [OPTION…] *PACKAGE*
   Validate a package against a virtual or physical device

   -s, --skip-checksums        Skip checksum verification for all input files
   --size=SIZE                 Size of virtual device. Default is 1GB
   -w, --write-image           Install package to virtual device as an additional validation step
   -b, --build-image=FILENAME  Build a bootable image from the package. Implies --write-image
   --device=DEVICE             Validate against provided device

   Available since: :ref:`release-2.0.0`

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

Devices with raw access to memory, not incorporating a flash translation layer,
like those accessible through the `Linux MTD interface
<http://www.linux-mtd.infradead.org/>`_, are currently not supported.
