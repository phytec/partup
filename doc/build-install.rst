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
-  `mtd-utils <http://linux-mtd.infradead.org/>`_

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

WSL (Windows Subsystem for Linux)
..................................

While partup only runs on Linux and does not support Microsoft Windows natively,
it can be run via WSL (Windows Subsystem for Linux). This section describes how
to setup WSL2 on a Windows machine, pass through a USB SD card reader and use
partup to flash an SD card, as an example.

Open the Windows command prompt as an administrator and install a WSL2
distribution::

   wsl --install --no-distribution
   wsl --install Ubuntu --version 2

Running a WSL2 distribution is as simple as clicking on the appropriate
shortcut, e.g. "Ubuntu", in the Windows start menu, or by running it from a
regular command prompt::

   wsl

For a detailed documentation, see the official instructions from Microsoft:
https://learn.microsoft.com/en-us/windows/wsl/install

Since WSL kernel version 6.6.29, USB storage support is built as a kernel module
and no further modifications to the WSL distribution have to be done.

.. note::
   For older WSL kernel releases prior to 6.6.29, USB storage support must be
   added manually by compiling your own version of the WSL kernel. See the
   offical Microsoft documentation on how to compile your kernel:
   https://learn.microsoft.com/en-us/community/content/wsl-user-msft-kernel-v6

   Add the following configuration options to a new file named ``usb.cfg`` in
   the WSL kernel source tree::

      CONFIG_USB=y
      CONFIG_USB_STORAGE=y
      CONFIG_MMC=y
      CONFIG_MMC_BLOCK=y
      CONFIG_MMC_SDHCI=y
      CONFIG_MMC_SDHCI_PCI=y

   Clone the WSL kernel, install dependencies, merge the ``usb.cfg`` config
   fragment and compile the kernel::

      sudo apt update
      sudo apt upgrade
      sudo apt install build-essential flex bison dwarves libssl-dev libelf-dev
      cpio qemu-utils pahole
      git clone https://github.com/microsoft/WSL2-Linux-Kernel.git --depth=1 -b linux-msft-wsl-5.15.y ~/kernel
      cd ~/kernel
      ./scripts/kconfig/merge_config.sh Microsoft/config-wsl usb.cfg
      make -j$(nproc) KCONFIG_CONFIG=Microsoft/config-wsl
      make INSTALL_MOD_PATH="$PWD/modules" modules_install
      cp arch/x86/boot/bzImage /mnt/c/

   Add the following content to the file ``%USERPROFILE%\.wslconfig``::

      [wsl2]
      kernel=C:\\bzImage

To pass through USB storage devices, like an SD card reader, to the WSL
distribution, install `usbipd-win <https://github.com/dorssel/usbipd-win>`_. As
an administrator in a Windows command prompt, do::

   winget install usbipd

Find the desired USB device and its bus ID::

   usbipd list

Bind the USB device (replace ``<BUSID>`` with the correct bus ID from above,
e.g. ``2-1``)::

   usbipd bind -b <BUSID>

Each time the WSL distribution is started, the USB device has to be attached. In
a regular Windows command prompt, execute::

   usbipd attach -w -b <BUSID>

To automatically attach a USB device usbipd, `wsl-usb-gui
<https://gitlab.com/alelec/wsl-usb-gui>`_ can be used. usbipd itself probably
`will not support this feature
<https://github.com/dorssel/usbipd-win/issues/371#issuecomment-1120202537>`_.
