Layout Configuration
====================

Common Sections
---------------

``api-version``
   The API version of the configuration file. This version must reflect the used
   feature API version used in the configuration file. If the API version of the
   configuration file is newer than the program version, partup exits with an
   error.

eMMC Sections
-------------

``disklabel``
   The disklabel as is accepted by parted. Currently support disklabel values
   include ``msdos`` and ``gpt``, though support for ``gpt`` is experimental.

``emmc-boot-partitions``
   A mapping describing the eMMC boot partitions.
