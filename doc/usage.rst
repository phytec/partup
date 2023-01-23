Usage
=====

General Usage
-------------

partup requires a configuration file specifying the layout and an output device
to write to as its mandatory arguments. An example looks like the following::

   partup -c layout.yaml /dev/mmcblk2

Option List
-----------

When executing partup, the following options can be specified:

-h, --help                  Show help options
-c, --config=CONFIG         Layout configuration file in YAML format
-d, --debug=DEBUG-DOMAINS   Comma separated list of modules for to enable debug output
-p, --prefix=PREFIX         Path to prefix all file URIs with in the layout configuration
-s, --skip-checksums        Skip checksum verification for all input files
-v, --version               Print the program version and exit
