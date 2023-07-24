Package Format
==============

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

The content of a package can be listed using the ``show`` command::

   partup show mypackage.partup

   u-boot.bin
   zImage
   rootfs.tar.gz
   layout.yaml

A partup package contains all the information needed to install the initial data
to a device. The ``install`` command then only needs the desired flash device to
be specified::

   partup install mypackage.partup /dev/mmcblk0
