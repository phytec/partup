.. image:: doc/data/partup-logo-2x.svg

partup - System Initialization Program
######################################

.. image:: https://github.com/phytec/partup/workflows/build/badge.svg
   :target: https://github.com/phytec/partup/actions/workflows/build.yml
.. image:: https://github.com/phytec/partup/workflows/tests/badge.svg
   :target: https://github.com/phytec/partup/actions/workflows/tests.yml
.. image:: https://github.com/phytec/partup/workflows/documentation/badge.svg
   :target: https://github.com/phytec/partup/actions/workflows/documentation.yml

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

Building
========

Build *partup* using the `Meson Build system <https://mesonbuild.com>`_::

   meson setup build
   meson compile -C build

Running
=======

Run *partup* by specifying the path to the layout configuration file and the
device that is being written to::

   partup -c /path/to/config.yaml /dev/mmcblk0

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

Install partup on the current system. If enabled, the documentation will be
installed as well::

   meson install -C build

Yocto Integration
-----------------

A Yocto recipe is available in `meta-ampliphy <https://git.phytec.de/meta-ampliphy/tree/recipes-devtools/partup>`_.

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
