Usage
=====

General Usage
-------------

partup requires a configuration file specifying the layout and an output device
to write to as its mandatory arguments. An example looks like the following::

   partup -c /usr/share/partup/imx8mp-emmc.yaml /dev/mmcblk2

Option List
-----------

When executing partup, the following options can be specified:

-h, --help              Show help options
-c, --config=CONFIG     Layout configuration file in YAML format
-d, --debug             Print debug messages
-v, --version           Print the program version and exit
