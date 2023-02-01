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

0.1.0
=====

*Release date: 2022-05-16*

Initial release of partup.
