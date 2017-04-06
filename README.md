USB Keyboard Skeleton
=====================
This repository includes a skeleton for implementing a USB keyboard
on a Teensy (Atmel AT90USB1286), compiled on Linux.

Documentation for the Atmel part and USB devices:

* [Atmel AT90USB64/128 Datasheet](www.atmel.com/images/doc7593.pdf)
* [Universal Serial Bus Specification, Revision 2.0](http://www.usb.org/developers/docs/usb20_docs/usb_20_033017.zip) (see usb_20.pdf)
* [Device Class Definition for Human Interface Devices (HID), Version 1.11](http://www.usb.org/developers/hidpage/HID1_11.pdf)
* [HID Usage Tables, Version 1.12](http://www.usb.org/developers/hidpage/Hut1_12v2.pdf)


USB keyboards report the state of the keys in one of three formats:

1. Modern BIOS and OSes recognize modifier byte, reserved byte,
   and N bytes of keycodes, zero padded. 
2. Legacy BIOS restricts the above by allowing only 6 bytes of keycodes.
3. Modern OSes recognize modifier byte, reserved byte, and N bytes
   of a bit array of key states.

This skeleton is configured to use the third format.


The report may be communicated to the USB host using one of three techniques
on the AT90USB1286:

1. as a response to HID 7.2.1 Get\_Report Request;
2. at Start of Frame interrupt;
3. at any time when interrupts are enabled.

This skeleton uses the third technique.
As committed, the skeleton simply sends the keycode for 'h' every second.


You'll need to have the Arduino build environment installed, as well
as the teensy\_loader\_cli tool to write the executable onto the Teensy.


Arduino Build Environment
-------------------------

    sudo apt-get install arduino-core
    sudo apt-get install arduino-mk
    sudo apt-get install libusb-dev
    sudo apt-get install gcc-avr binutils-avr avr-libc

teensy\_loader\_cli
-------------------

    git clone https://github.com/PaulStoffregen/teensy_loader_cli.git
    cd teensy_loader_cli
    make
    sudo cp teensy_loader_cli /usr/local/bin/

teensy\_loader\_cli requires a UDEV rule to access the USB port without
root access.  You can install this directly by:

    sudo curl -s https://www.pjrc.com/teensy/49-teensy.rules > /etc/udev/rules.d/49-teensy.rules


Building
--------

    git clone https://github.com/ktavenner/usb_keyboard.git
    cd usb_keyboard
    make && make load

Debugging
---------

    lsusb                   ## list USB devices
    dmesg -w                ## trace USB discovery
    tcpdump -xx -i usbmon0  ## trace USB messages
