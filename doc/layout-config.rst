Layout Configuration
====================

API Version
-----------

``api-version`` (integer)
   The API version used in this configuration file. The provided number must be
   equal or smaller than the major version of the currently used partup
   executable.

   This scalar is mandatory.

MMC Options
-----------

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
   A partition label.

``type`` (string)
   The partition type. May be one of ``primary`` or ``logical``. Note, that with
   the first occurrence of a logical partition the following ones must be
   logical, too.

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

``input`` (sequence)
   A sequence of input mappings. See :ref:`input-files`.

eMMC Specifics
--------------

eMMC Boot Partitions
....................

eMMC's special boot partitions can be specified using the keyword
``emmc-boot-partitions`` containing a mapping of the following options:

``enable`` (boolean)
   Enable the boot partitions.

``input-offset`` (integer/string)
   Offset of the input data to be written.

``output-offset`` (integer/string)
   Offset of the written output data.

``input`` (mapping)
   An input mapping. See :ref:`input-files`.

.. _input-files:

Input Files
-----------

Input files are specified by a scalar named ``input`` containing a mapping with
at least a ``uri``. For verifying the checksum of the given file by ``uri``, an
optional checksum can be provided with ``md5sum`` and/or ``sha256sum``.

``uri`` (string)
   A valid URI pointing to a file that should be written to the parent partition
   or volume. Currently supported URI schemes are:

   * ``file://`` with an absolute or relative path to a local file.

``md5sum`` (string)
   The MD5 sum of the given file specified by ``uri``. This sum is checked
   against the written file on the target partition or volume.

``sha256sum`` (string)
   The SHA256 sum of the given file specified by ``uri``. This sum is checked
   against the written file on the target partition or volume.
