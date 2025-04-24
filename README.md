Blinky
======

A simple blinky program that flashes and outputs console data.

prog-1
======

A full demonstration of flashing an RP2040 via SWD.

Flash Test 1
============

This demonstrates the ability to transfer a complete program from RAM
into flash and restart.

The program that is flashed is blinky.cpp.  A few things need to happen
to prepare this test

* Create the makefile for the correct target:

        cmake -DPICO_BOARD=pico ..

* Build blinky normally to create blinky.elf

        make blinky

* Convert blinky.elf to blinky.bin using:

         arm-none-eabi-objcopy -O binary blinky.elf blinky.bin

* Convert the blinky.bin to a C header file that will be included in the test.

        xxd -g4 -i blinky.bin > ../blinky-bin-rp2040.h

The blinky binary will be loaded into the base of the flash (XIP) and then 
a processor reset will be invoked.

Here's a helpful command to create the disassembly listing:

        arm-none-eabi-objdump -S blinky.elf > blinky.lst
        
