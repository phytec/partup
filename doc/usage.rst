Usage
=====

General Usage
-------------

partup requires a configuration file specifying the layout and an output device
to write to as its mandatory arguments. The full syntax is as follows::

   partup [OPTION?] -c CONFIG DEVICE

An minimal example may look like the following::

   partup -c layout.yaml /dev/mmcblk2

Option List
-----------

When executing partup, the following options can be specified:

-h, --help              Show help options
-c, --config=CONFIG     Layout configuration file in YAML format
-d, --debug             Print debug messages
-p, --prefix=PREFIX     Path to prefix all file URIs with in the layout configuration
-s, --skip-checksums    Skip checksum verification for all input files
-v, --version           Print the program version and exit
