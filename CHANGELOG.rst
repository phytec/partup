.. _release-2.2.0:

2.2.0
=====

*Release date: 2024-11-14*

.. rubric:: Changes

-  Allow an empty disklabel and thus skipping partitioning. This also disables
   any checks whether written raw data would overwrite the partition table and
   any partitions. Setting eMMC-specific features is still possible in this
   mode.
-  Optimize the statically built binaries of partup by building with the release
   type and stripping symbols after it.

.. rubric:: Contributors

`Martin Schwan <https://github.com/mschwan-phytec>`__

.. _release-2.1.0:

2.1.0
=====

*Release date: 2024-10-09*

.. rubric:: Changes

-  Verify the checksum of written data, independent of a provided input
   checksum.
-  Reorder some file-related helper functions into their own module. There were
   no functional changes in this case.

.. rubric:: Contributors

`Martin Schwan <https://github.com/mschwan-phytec>`__

.. _release-2.0.0:

2.0.0
=====

*Release date: 2024-02-22*

.. rubric:: Changes

-  Allow setting the eMMC boot acknowledgement bit.
-  Allow enabling or disabling the eMMC H/W reset feature bit. This is a
   one-time programmable setting.
-  Allow setting the eMMC bootbus conditions.
-  Print the layout configuration when using ``partup show``.
-  Add support for the FAT16 filesystem type.
-  Allow passing extra arguments to mkfs when making the filesystem on a
   partition with the option ``mkfs-extra-args``.
-  Print the help text when no command is provided to partup.

.. rubric:: Contributors

`Martin Schwan <https://github.com/mschwan-phytec>`__,
`Leonard Anderweit <https://github.com/landerweit-phytec>`__

.. _release-1.0.0:

1.0.0
=====

*Release date: 2023-11-28*

.. rubric:: Changes

-  Host partup's documentation on readthedocs.org.
-  Add usage description for all commands.
-  The layout configuration file does not need to be named ``layout.yaml``
   anymore. Any yaml file provided will be recognized as the layout.
-  Rename the input uri to filename as all files are provided in a package and
   therefore only need a relative path.
-  Set the filesystem label after writing raw ext[234] partition. This
   overwrites the label of the provided filesystem unless label is set to null.
-  Restructure the way eMMC boot partitions are specified in the layout
   configuration, so other MMC controls can be added in the future. Make the
   input for eMMC boot partitions optional, allowing to disable them without any
   input.
-  Only write eMMC boot partitions if they actually exist. SD cards do not
   support eMMC boot partitions, so they should be ignored for this option.
-  Allow setting the eMMC boot partition to be enabled by its number.
   Previously, only a boolean value was allowed, but now it is possible to
   specify the exact boot partition to enable.
-  Multiple binaries for the eMMC boot partitions can now be specified. Each
   with their own input and output offset.

.. rubric:: Bug Fixes

-  Set LABEL for supported filesystems during filesystem creation. Previously
   only PARTLABEL would be set, but this is only supported with GPT.

.. rubric:: Contributors

`Martin Schwan <https://github.com/mschwan-phytec>`__,
`Leonard Anderweit <https://github.com/landerweit-phytec>`__

.. _release-0.4.0:

0.4.0
=====

*Release date: 2023-07-13*

.. rubric:: Changes

-  Implement a new module "command" that allows creating a context for parsing
   command line positional arguments. Each command can have their own
   command-specific options. Use these commands in the main program and move
   most functionality to commands, except for the automatically generated help
   option. Also provide commands for inspecting, creating and installing the new
   partup packages.
-  Add a new module for creating and listing content of partup packages. partup
   packages are just SquashFS images under the hood, but have some specific
   requirements that allow for an easy way of providing input files and flash
   layout configurations.
-  Check, if raw input data would overwrite the partition table or any other
   data specified in the layout configuration.
-  Add program option ``--quiet`` to disable message printing.
-  Add option ``--debug-domains`` to specify which debug output is printed.
-  Improve debug and normal message printing.
-  Add a custom logging writer that can format output messages with color, if
   supported. The output format is slightly tuned from the default coming from
   GLib, to be shorter and easier to read.
-  Remove API documentation generated with Doxygen and Breathe. Since partup
   does not provide a library, this type of documentation is not needed.
-  Describe the main features of partup in the documentation index page.
-  Use libmount from util-linux for mount operations instead of calling mount
   and umount in a command line.
-  Use libblkid for detecting drives instead of calling similar commands from a
   command line.
-  Add a build option enabling to statically link GLib and GIO to partup. This
   is useful to support older systems, where the version of GLib may be too old
   and does not contain required functions.
-  Add more unit tests for various modules.
-  Add unit tests for functions requiring root access.

.. rubric:: Bug Fixes

-  Fix mount detection on devices without partitions.

.. rubric:: Contributors

`Martin Schwan <https://github.com/mschwan-phytec>`__,
`Leonard Anderweit <https://github.com/landerweit-phytec>`__

.. _release-0.3.1:

0.3.1
=====

*Release date: 2023-03-03*

.. rubric:: Changes

-  Correct a spelling mistake in the configuration reference.
-  Print the output message of failed processes spawned by partup.

.. rubric:: Bug Fixes

-  Fix a bug where partup would not return when specifying an ``input-offset``
   greater than zero.
-  Wait for partitions to show up after setting up the partition layout. This
   prevents premature writing to partitions not yet known to the operating
   system.

.. rubric:: Contributors

`Leonard Anderweit <https://github.com/landerweit-phytec>`__,
`Martin Schwan <https://github.com/mschwan-phytec>`__

.. _release-0.3.0:

0.3.0
=====

*Release date: 2023-02-01*

.. rubric:: Changes

-  Add support for GUID Partition Tables. This includes setting a UUID for each
   partition using the ``partuuid`` option.
-  Add a new configuration option ``clean`` for cleaning specific sections
   outside of partitions. This is especially useful to clean previously existing
   bootloader environments.
-  Add a new configuration option ``flags`` for partitions. This makes it
   possible to specify partition `flags, as defined by parted
   <https://www.gnu.org/software/parted/manual/html_node/set.html>`__.
-  Add new configuration option ``block-size`` for partitions, to make sure a
   partition's size is a multiple of this value.
-  Add various checks for:

   -  using a correct drive instead of existing partitions.
   -  a correct offset of the first partition to not overwrite the partition
      table.
   -  empty ``uri`` values in ``input``.
   -  missing inputs for ``raw`` and ``emmc-boot-partitions``.

-  Add option ``-s, --skip-checksums`` to skip checksum verification of input
   files.
-  Add option ``-p, --prefix=PREFIX`` to add a prefix path to all file-type
   input URIs.
-  Add support for writing ext[234] images to an unformatted partition.
-  Resize the filesystem after writing ext[234] files
-  Consistently verify the checksum of files *before* writing them.
-  Improve README by listing dependencies and using Meson Build commands only.
   Add notes about install partup.
-  Various improvements and additions to the documentation:

   -  Document the supported file types for the ``input`` option.
   -  Describe the supported output devices.
   -  Document the supported partition tables used in the ``disklabel`` option.
   -  Add examples of the layout configuration file.
   -  Add a nice logo for partup.

.. rubric:: Bug Fixes

-  Make it possible to use ``/dev/sd*`` named output devices.
-  Unmount dangling partitions in case of a write failure.
-  Correctly free an internal object holding a list of input files.
-  Only mount partitions when needed during writing.
-  Check exit status of spawned processes to uncover any errors.

.. rubric:: Contributors

`Martin Schwan <https://github.com/mschwan-phytec>`__,
`Leonard Anderweit <https://github.com/landerweit-phytec>`__

.. _release-0.2.1:

0.2.1
=====

*Release date: 2022-11-11*

.. rubric:: Changes

-  Improve the writing of raw data using GLib's streaming function instead of
   calling a subprocess using ``dd``.
-  Improve error handling of the utilities functions.

.. rubric:: Bug Fixes

-  Increase the padding of logical partitions to two sectors to allow storing the
   EBR information. Previously, logical partitions may have overlapped because
   the padding was too narrow.

.. _release-0.2.0:

0.2.0
=====

*Release date: 2022-09-06*

.. rubric:: Changes

-  Refactor the way configuration files are being parsed. Now, the ``PuConfig``
   object parses the configuration file in a generic manner. The conversion to a
   correct layout description is handled by the flash object implementation,
   e.g. ``PuEmmc``.
-  Add support for more advanced input data handling with the ``input`` option.
-  Add optional checksum verification for input data.
-  Add unit tests for the following modules: ``config``, ``emmc`` and
   ``checksum``.
-  Add a short documentation about possible keys and values of the configuration
   file.
-  Allow backwards-compatibility of older configuration files by modifying
   partup's API version checker.
-  Add an explicit option ``expand`` to allow for a partition to expand equally
   for the remaining space.
-  Add support for size units, e.g. ``MiB``, ``s`` and ``kB``. The supported
   units are the same as being used in libparted.

.. rubric:: Bug Fixes

-  Fix a floating point exception when no expanding partitions were specified.
   This bug occured because with no expanding partition a division by zero would
   be executed.

.. _release-0.1.0:

0.1.0
=====

*Release date: 2022-05-16*

Initial release of partup.
