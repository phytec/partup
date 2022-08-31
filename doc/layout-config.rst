Layout Configuration
====================

API Version
-----------

MMC Options
-----------

eMMC Specifics
..............

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
