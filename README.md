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
        
Register Analysis
=================

After reset:

        PC    000000EA
        LR    FFFFFFFF
        XSPR  F1000000
        r0    FFFFFFFF
        AIRCR FA050000
        ICSR  00400000 (Interrupt control and state register)
                ISRPENDING set
        ICPR  00018020
        DHCSR 0003000B
        DFSR  00000009
        DEMCR 00000001 

ICSR has 0000_0000_0100_0000_0000_0000_0000, has 22 bit on. This indicates that an external interrupt is pending.
But since interrupts are masked (bit 4 on DHCSR) this shoudln't make any difference.  Note that bit 23 (ISRPREEMPT)
is not set, so this seems to indicates that there will be no interrupt fired when leaving debug state,

DFSR has the 3 bit and the 0 bit set, which means (3) a vector catch occurred (0) a halt was requestd by the DAP.

DEMCR has the 0 bit set, which tells the processor to enter debug on reset.

Single Step 
===========

----- After Reset -----
PC=000000EA, LR=FFFFFFFF, MSP=20041F00
XSPR  F1000000
CTL/PRIMASK  00000000
r0=FFFFFFFF, r7=20041F00
AIRCR FA050000
ICSR  00400000
  ISRPENDING set
ICPR  00018040
DHCSR 0003000B
DFSR  00000009
DEMCR 00000001
@0    20041F00
@4    000000EB
Calling 20000011 ( 00000000, 00000000, 00000000, 00000000)

Before Step
PC=20000010, LR=FFFFFFFF, MSP=20000080
XSPR  01000000
CTL/PRIMASK  00000001
r0=00000000, r7=20000080
AIRCR FA050000
ICSR  00400000
  ISRPENDING set
ICPR  00018040
DHCSR 0003000B
DFSR  00000000
DEMCR 00000001
@0    20041F00
@4    000000EB

After Step
PC=20000012, LR=FFFFFFFF, MSP=20000080
XSPR  01000000
CTL/PRIMASK  00000001
r0=00000008, r7=20000080
AIRCR FA050000
ICSR  00400000
  ISRPENDING set
ICPR  00018040
DHCSR 0003000F
DFSR  00000001
DEMCR 00000001
@0    20041F00
@4    000000EB
Resetting ...
Blocking

----- After Reset -----
PC=000000EA, LR=FFFFFFFF, MSP=20041F00
XSPR  F1000000
CTL/PRIMASK  00000000
r0=FFFFFFFF, r7=20041F00
AIRCR FA050000
ICSR  00400000
  ISRPENDING set
ICPR  00018040
DHCSR 0003000B
DFSR  00000009
DEMCR 00000001
@0    20041F00
@4    000000EB
Calling 20000011 ( 00000000, 00000000, 00000000, 00000000)

Before Step
PC=20000010, LR=FFFFFFFF, MSP=20000080
XSPR  01000000
CTL/PRIMASK  00000001
r0=00000000, r7=20000080
AIRCR FA050000
ICSR  00400000
  ISRPENDING set
ICPR  00018040
DHCSR 0003000B
DFSR  00000000
DEMCR 00000001
@0    20041F00
@4    000000EB

After Step
PC=20000012, LR=FFFFFFFF, MSP=20000080
XSPR  01000000
CTL/PRIMASK  00000001
r0=00000008, r7=20000080
AIRCR FA050000
ICSR  00400000
  ISRPENDING set
ICPR  00018040
DHCSR 0003000F
DFSR  00000001
DEMCR 00000001
@0    20041F00
@4    000000EB
Resetting ...
Blocking

