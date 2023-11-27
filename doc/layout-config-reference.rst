Layout Configuration Reference
==============================

API Version
-----------

``api-version`` (integer)
   The API version used in this configuration file. The provided number must be
   equal or smaller than the major version of the currently used partup
   executable.

   This scalar is mandatory.

Disk Options
------------

``disklabel`` (string)
   Partition table for the device. Currently supported options are ``msdos``
   (default) and ``gpt``.

Clean Data
..........

The section ``clean`` contains a sequence of mappings describing where the mmc
device is cleaned outside of partitions. Each entry should contain the
following options:

``offset`` (integer/string)
   Offset of the cleaned space.

``size`` (integer/string)
   Size of the cleaned space.

Raw Data
........

The section ``raw`` contains a sequence of mappings describing data that is
written outside of partitions. Each entry may contain the following options:

``input-offset`` (integer/string)
   Offset of the input data to be written.

``output-offset`` (integer/string)
   Offset of the written output data.

``input`` (mapping)
   An input mapping. See :ref:`input-files`.

Partitions
..........

The section ``partitions`` contains a sequence of mappings describing the
contained partitions of an MMC device. Each entry may contain the following
options:

``label`` (string)
   A partition label. The label is set in the filesystem of the partition and in
   the partition table if GPT is used. For raw ext[234] images, providing a
   label overwrites the label included in the filesystem image.
   The default value is ``null``.

``partuuid`` (string)
   The PARTUUID of the partition. Only supported on GPT partitioned devices. A
   random UUID is used by default.

``type`` (string)
   The partition type. May be one of ``primary`` or ``logical``. Note, that with
   the first occurrence of a logical partition the following ones must be
   logical, too. Logical partitions are only supported with the ``msdos``
   partition table.

``filessytem`` (string)
   The filesystem type to use during formatting of the partition, e.g. ``fat32``
   or ``ext4``.

``size`` (integer/string)
   The size of the partition.

``expand`` (boolean)
   Expands the partition to fill the rest of the flash device. If multiple
   partitions are specified to be expanding, then the space is equally divided
   between them. By default, partitions do not expand.

``offset`` (integer/string)
   The offset of a partition.

``block-size`` (integer/string)
   Set the partition size to a multiple of the specified value. The default is
   2 sectors, which is almost always equal to 1KiB.

``flags`` (sequence)
   Set flags for this partition. Flags to be enabled should be provided as a
   sequence of strings. Possible flags are the same as specified by
   `GNU parted's set command <https://www.gnu.org/software/parted/manual/parted.html#set>`_.

``input`` (sequence)
   A sequence of input mappings. See :ref:`input-files`.

MMC Controls
------------

MMC specific controls can be specified using the keyword ``mmc`` containing a
mapping of the following options:

``boot-partitions`` (mapping)
   An eMMC boot partitions mapping. See :ref:`boot-partitions`.

.. _boot-partitions:

eMMC Boot Partitions
....................

eMMC's special boot partitions can be specified using the keyword
``boot-partitions`` containing a mapping of the following options:

``enable`` (integer)
   Enable and select the boot partition. 0 to disable boot partitions.

``binaries`` (sequence)
   A sequence of binaries to copy to the boot partitions. See :ref:`binaries`.
   This keyword is optional.

.. _binaries:

.. rubric:: Binaries

Binary files are specified by a scalar named ``binaries`` containing a sequence
of mappings with at least an ``input``.

``input-offset`` (integer/string)
   Offset of the input data to be written. This keyword is optional.

``output-offset`` (integer/string)
   Offset of the written output data. This keyword is optional.

``input`` (mapping)
   An input mapping. See :ref:`input-files`.

.. _input-files:

Input Files
-----------

Input files are specified by a scalar named ``input`` containing a mapping with
at least a ``filename``. For verifying the checksum of the given file by
``filename``, an optional checksum can be provided with ``md5sum`` and/or
``sha256sum``.

``filename`` (string)
   A valid relativ path pointing to a file that should be written to the parent
   partition or volume.

``md5sum`` (string)
   The MD5 sum of the given file specified by ``filename``. This sum is checked
   against the provided file before writing to the target partition or volume.

``sha256sum`` (string)
   The SHA256 sum of the given file specified by ``filename``. This sum is
   checked against the provided file before writing to the target partition or
   volume.

Supported File Types
....................

The provided input files are copied to the filesystem of the corresponding
partition by default. However, if files are of one of the supported special file
types, they are treated as following.

``tar`` or ``tar.*``
   Archives and compressed archives are extracted into the filesystem.

``ext[234]``
   Raw filesystem files are written directly to the partition. This overrides
   any existing filesystem, so it should be specified as ``filesystem: null`` or
   not be specified at all. Additionally ext filesystems are resized to utilize
   the whole partition.
