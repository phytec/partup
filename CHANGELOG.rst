0.2.0
=====

*Release date: 2022-09-06*

.. rubric:: Changes

* Refactor the way configuration files are being parsed. Now, the ``PuConfig``
  object parses the configuration file in a generic manner. The conversion to a
  correct layout description is handled by the flash object implementation, e.g.
  ``PuEmmc``.
* Add support for more advanced input data handling with the ``input`` option.
* Add optional checksum verification for input data.
* Add unit tests for the following modules: ``config``, ``emmc`` and
  ``checksum``.
* Add a short documentation about possible keys and values of the configuration
  file.
* Allow backwards-compatibility of older configuration files by modifying
  partup's API version checker.
* Add an explicit option ``expand`` to allow for a partition to expand equally
  for the remaining space.
* Add support for size units, e.g. ``MiB``, ``s`` and ``kB``. The supported
  units are the same as being used in libparted.

.. rubric:: Bug Fixes

* Fix a floating point exception when no expanding partitions were specified.
  This bug occured because with no expanding partition a division by zero would
  be executed.

0.1.0
=====

*Release date: 2022-05-16*

Initial release of partup.
