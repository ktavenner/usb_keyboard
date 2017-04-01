USB Keyboard Skeleton
=====================
This repository includes a skeleton for implementing a USB keyboard
on an Teensy (Atmel AT90USB1286), compiled on Linux.

You'll need to have the Arduino build environment installed, as well
as the teensy_loader_cli tool to write the executable onto the Teensy.


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
