USB Keyboard Skeleton
=====================
This repository includes a skeleton for implementing a USB keyboard
on a Teensy (Atmel AT90USB1286), compiled on Linux.

USB keyboards report the state of the keys in one of three formats:

1. Modern BIOS and OSes recognize modifier byte, reserved byte,
   and N bytes of keycodes, zero padded. 
2. Legacy BIOS restricts the above by allowing only 6 bytes of keycodes.
3. Modern OSes recognize modifier byte, reserved byte, and N bytes
   of a bit array of key states.

This skeleton is configured to use the third format.

The report may be communicated to the USB host using one of three techniques
on the AT90USB1286:

1. as a response to HID 7.2.1 Get_Report Request;
2. at Start of Frame interrupt;
3. at any time when interrupts are enabled.

This skeleton uses the third technique.

You'll need to have the Arduino build environment installed, as well
as the `teensy_loader_cli` tool to write the executable onto the Teensy.


Arduino Build Environment
-------------------------

    sudo apt-get install arduino-core
    sudo apt-get install arduino-mk
    sudo apt-get install libusb-dev
    sudo apt-get install gcc-avr binutils-avr avr-libc

teensy_loader_cli
-----------------

    git clone https://github.com/PaulStoffregen/teensy_loader_cli.git
    cd teensy_loader_cli
    make
    sudo cp teensy_loader_cli /usr/local/bin/

Building
--------

    make && make load

Debugging
---------

    lsusb
    dmesg -w
    tcpdump -I usbmon0
