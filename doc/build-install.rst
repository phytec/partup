Building and Installing partup
==============================

Dependencies
------------

The following tools and libraries must be installed on the target system for
partup to run:

-  `GLib <https://docs.gtk.org/glib/>`_ (minimum version 2.66.0)
-  `libyaml <https://pyyaml.org/wiki/LibYAML>`_
-  `libparted <https://www.gnu.org/software/parted/>`_
-  `util-linux <https://git.kernel.org/pub/scm/utils/util-linux/util-linux.git>`_
-  `udev <https://git.kernel.org/pub/scm/linux/hotplug/udev.git>`_
-  `squashfs-tools <https://github.com/plougher/squashfs-tools>`_
-  `dosfstools <https://github.com/dosfstools/dosfstools>`_
-  `e2fsprogs <https://git.kernel.org/pub/scm/fs/ext2/e2fsprogs.git>`_

For building partup from source and generating its documentation the following
additional dependencies are needed:

-  `Meson Build <https://mesonbuild.com/>`_
-  `Sphinx <https://www.sphinx-doc.org/>`_

Installing Dependencies
-----------------------

Install the following dependencies with your package manager:

Ubuntu and Debian
.................

::

   apt-get install libglib2.0-dev libyaml-dev libparted-dev util-linux udev \
                   squashfs-tools dosfstools e2fsprogs meson python3 \
                   python3-virtualenv

Arch Linux
..........

::

   pacman -S glib2 libyaml parted util-linux squashfs-tools dosfstools \
             e2fsprogs meson python python-virtualenv

Building partup
---------------

Build partup using the `Meson Build system <https://mesonbuild.com>`_::

   meson setup build
   meson compile -C build

Building the Documentation
--------------------------

Install the dependencies needed for building the documentation in a Python
virtualenv::

   python3 -m venv env
   source env/bin/activate
   pip install -r doc/requirements.txt

Configure the build directory for building the documentation::

   meson setup --reconfigure build -Ddoc=true

Build the documentation::

   meson compile -C build doc

Installing partup
-----------------

From Source
...........

Install partup on the current system. If enabled, the documentation will be
installed as well::

   meson install -C build

Ubuntu and Debian
.................

For Debian-based systems (and most other Linux Distributions), it is easiest to
install partup by downloading the latest statically built binary from the
release page: https://github.com/phytec/partup/releases

Install the downloaded binary to a directory contained in ``$PATH``, e.g.
``/usr/local/bin``::

   install -m 755 ~/Downloads/partup-v0.0.0-amd64 /usr/local/bin/

Make sure to specify the correct path you downloaded partup to, as the above is
just an example.

Arch Linux
..........

partup is available in the AUR (Arch Linux User Repository):
https://aur.archlinux.org/packages/partup

Yocto
.....

A Yocto recipe is available in `meta-phytec
<https://git.phytec.de/meta-phytec/tree/recipes-support/partup>`_.
