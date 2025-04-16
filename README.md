Main
====

A demonstration of connecting to a Pico via the SWD debug port.

Blinky
======

A simply blinky program that flashes and outputs console data.

Flash Test 1
============

This demonstrates the ability to transfer a complete program from RAM
into flash and restart.

The program that is flashed is blinky.cpp.  A few things need to happen
to prepare this test

* Build blinky normally to create blinky.elf
* Convert blinky.elf to blinky.bin using:

         arm-none-eabi-objcopy -O binary blinky.elf blinky.bin

* Convert the blinky.bin to a C header file that will be included in the test.

        xxd -g4 -i blinky.bin > ../blinky-bin.h

The blinky binary will be loaded into the base of the flash (XIP) and then 
a processor reset will be invoked.

Here's a helpful command to create the disassembly listing:

        arm-none-eabi-objdump -S blinky.elf > blinky.lst
        