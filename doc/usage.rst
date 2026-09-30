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

   -s, --skip-checksums    Skip all checksum verification. This disables both
                           the optional verification of input files (``md5sum``
                           and ``sha256sum``) *before* writing and the automatic
                           SHA1 read-back verification of written raw data
                           *after* writing. See :ref:`checksum-verification`.

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

.. _checksum-verification:

Checksum Verification
---------------------

During ``partup install`` two independent checksum mechanisms are used to protect
the integrity of the data. Both can be disabled at once by passing the
``-s``/``--skip-checksums`` runtime argument to the ``install`` command.

Input Verification (before writing)
...................................

Before any data is written to the target device, partup can verify that the input
files bundled in the package are intact. This verification is *optional* and only
happens for the checksums that are provided in the :doc:`layout configuration
<layout-config-reference>`:

-  ``md5sum`` -- the MD5 sum of the input file (see :ref:`input-files`).
-  ``sha256sum`` -- the SHA256 sum of the input file (see :ref:`input-files`).

For every input file that specifies one or both of these options, partup computes
the corresponding checksum over the whole file and compares it against the value
from the layout configuration. If a checksum does not match, the installation is
aborted before anything is written. Input files that do not specify a checksum
are not verified at this stage.

This verification applies to all input files, regardless of how they are written,
i.e. files copied into a filesystem, archives extracted into a filesystem, raw
filesystem images, raw binaries and eMMC boot partition binaries.

The input verification is skipped entirely when ``--skip-checksums`` is given.

Output Verification (read-back after writing)
.............................................

In addition to the optional input verification, partup *automatically* verifies
that raw data has been written correctly by reading it back from the device.
This mechanism does not require any configuration and works as follows:

1. A SHA1 sum is computed from the input file (honoring any given
   ``input-offset``).
2. After the data has been written, the same range is read back from the target
   device (honoring any given ``output-offset``).
3. The SHA1 sum of the data read back from the device is compared against the
   SHA1 sum computed from the input file.

This read-back verification is independent from the ``md5sum`` and ``sha256sum``
options and always uses SHA1. It is applied only to writes that go directly to
the raw device, namely:

-  the ``raw`` section of MMC and HD devices (since :ref:`release-2.1.0`),
-  eMMC boot partition ``binaries`` -- verified on both boot partitions
   (``boot0`` and ``boot1``) (since :ref:`release-3.0.0`), and
-  MTD ``partitions`` (since :ref:`release-3.0.0`).

Read-back verification is *not* performed for data that is written through a
mounted filesystem, i.e. files copied into a partition, archives extracted into a
partition, or raw ext[234] filesystem images written to a partition.

The output verification is skipped when ``--skip-checksums`` is given.
