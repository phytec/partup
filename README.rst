.. image:: doc/data/partup-logo-2x.svg

partup - System Initialization Program
######################################

.. image:: https://github.com/phytec/partup/workflows/build/badge.svg
   :target: https://github.com/phytec/partup/actions/workflows/build.yml
.. image:: https://github.com/phytec/partup/workflows/unit-tests/badge.svg
   :target: https://github.com/phytec/partup/actions/workflows/unit-tests.yml
.. image:: https://github.com/phytec/partup/workflows/system-tests/badge.svg
   :target: https://github.com/phytec/partup/actions/workflows/system-tests.yml
.. image:: https://readthedocs.org/projects/partup/badge/?version=latest
   :target: https://partup.readthedocs.io/en/latest/?badge=latest
   :alt: Documentation Status

+------+------------------------------------------------------------------+
| NOTE | *partup* is in "beta" state and not yet considered stable. The   |
|      | configuration API may change without notice. Other parts of the  |
|      | program may also change drastically until release version 1.0.0. |
+------+------------------------------------------------------------------+

Dependencies
============

The following tools and libraries must be installed on the target system for
*partup* to run:

-  `GLib <https://docs.gtk.org/glib/>`_ (minimum version 2.66.0)
-  `libyaml <https://pyyaml.org/wiki/LibYAML>`_
-  `libparted <https://www.gnu.org/software/parted/>`_
-  `util-linux <https://git.kernel.org/pub/scm/utils/util-linux/util-linux.git>`_
-  `udev <https://git.kernel.org/pub/scm/linux/hotplug/udev.git>`_
-  `squashfs-tools <https://github.com/plougher/squashfs-tools>`_
-  `dosfstools <https://github.com/dosfstools/dosfstools>`_
-  `e2fsprogs <https://git.kernel.org/pub/scm/fs/ext2/e2fsprogs.git>`_

For building *partup* from source and generating its documentation the following
additional dependencies are needed:

-  `Meson Build <https://mesonbuild.com/>`_
-  `Sphinx <https://www.sphinx-doc.org/>`_

Building
========

Build *partup* using the `Meson Build system <https://mesonbuild.com>`_::

   meson setup build
   meson compile -C build

Running
=======

*partup* works in two steps:

#. Create a *partup* package by specifying a package name, the layout
   configuration file and all files that should be written to the target::

      partup package mypackage.partup u-boot.bin zImage rootfs.tar.gz layout.yaml

#. Install the package to the target::

      partup install mypackage.partup /dev/mmcblk0

Documentation
=============

Install the dependencies needed for building the documentation::

   virtualenv -v env
   source env/bin/activate
   pip install -r doc/requirements.txt

Configure the build directory for building the documentation::

   meson setup --reconfigure build -Ddoc=true

Build the documentation::

   meson compile -C build doc

Installation
============

From Source
-----------

Install partup on the current system. If enabled, the documentation will be
installed as well::

   meson install -C build

Arch Linux
----------

partup is available in the AUR (Arch Linux User Repository):
https://aur.archlinux.org/packages/partup

Yocto
-----

A Yocto recipe is available in `meta-ampliphy
<https://git.phytec.de/meta-ampliphy/tree/recipes-devtools/partup>`_.

License
=======

*partup* is licensed under the `GNU General Public License version 3
<https://www.gnu.org/licenses/gpl-3.0.en.html>`_. See ``LICENSE`` for more
information.

   partup - System Initialization Program

   Copyright (C) 2023  PHYTEC Messtechnik GmbH

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
