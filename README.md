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

* Create the makefile for the correct target:

        cmake -DPICO_BOARD=pico ..

* Build blinky normally to create blinky.elf

        make blinky

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
# NOTE: The PC and MSP have been loaded from the default (ROM)
# Vector Table. LR is in the startup value.
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
# Indicates that we want a vector catch (reset)
DEMCR 00000001
@0    20041F00
@4    000000EB
Calling 20000011 ( 00000000, 00000000, 00000000, 00000000)

Before Single Step
# PC and MSP set explicitly
PC=20000010, LR=FFFFFFFF, MSP=20000080
XSPR  01000000
CTL/PRIMASK  00000001
r0=00000000, r7=20000080
AIRCR FA050000
ICSR  00400000
  ISRPENDING set
ICPR  00018040
# Same value as we had after reset (1011, mask int, halt, debug)
DHCSR 0003000B
# Cleared explicitly
DFSR  00000000
# Indicates that we want a vector catch (reset)
DEMCR 00000001
# ROM Values
@0    20041F00
@4    000000EB

----- After Single Step -----
# PC has advanced one instruction
PC=20000012, LR=FFFFFFFF, MSP=20000080
XSPR  01000000
CTL/PRIMASK  00000001
# r0 sees the impact of the MOV
r0=00000008, r7=20000080
AIRCR FA050000
ICSR  00400000
  ISRPENDING set
ICPR  00018040
# (1111, mask int, step, halt, debug)
DHCSR 0003000F
# The halt bit is set (expected)
DFSR  00000001
# Indicates that we want a vector catch (reset)
DEMCR 00000001
@0    20041F00
@4    000000EB
Resetting ...
Blocking

Running a Simple Program Into a BKPT
====================================

    mov r0, #8
    bkpt #0


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

        Calling 20000011 ( 00000000, 00000000, 00000000, 00000000)

        ----- Before Step -----
        PC=20000010, LR=FFFFFFFF, MSP=20000080
        XSPR  01000000
        CTL/PRIMASK  00000001
        r0=00000000, r7=20000080
        AIRCR FA050000
        ICSR  00400000
                ISRPENDING set
        ICPR  00018040
        # The C_HALT bit is set (caused by the reset)
        DHCSR 0003000B
        DFSR  00000000
        DEMCR 00000001

        ----- After Step -----
        # PC moved past the MOV, but not past the BKPT
        PC=20000012, LR=FFFFFFFF, MSP=20000080
        XSPR  01000000
        CTL/PRIMASK  00000001
        # r0 set successfully
        r0=00000008, r7=20000080
        AIRCR FA050000
        ICSR  00400000
                ISRPENDING set
        ICPR  00018040
        # The C_HALT bit is set (caused by the breakpoint), but the 
        # C_STEP bit is not set
        DHCSR 0003000B
        # The BKPT bit is set here
        DFSR  00000002
        DEMCR 00000001

Running a Mini Program Via the Debug Trampoline
===============================================

This program is loaded into RAM at 0x20000010:

        mov r0, #8
        bx lr

        ----- After Reset -----
        PC=000000EA, LR=FFFFFFFF, MSP=20041F00
        XSPR  F1000000
        CTL/PRIMASK  00000000
        r0=FFFFFFFF, r7=FFFFFFFF
        AIRCR FA050000
        ICSR  00400000
                ISRPENDING set
        ICPR  00018040
        DHCSR 0003000B
        DFSR  0000000B
        DEMCR 00000001

        Calling 20000011 ( 00000000, 00000000, 00000000, 00000000)

        ----- Before Resume -----
        # PC pointing to trampoline
        PC=000001A8, LR=FFFFFFFF, MSP=20000080
        XSPR  01000000
        CTL/PRIMASK  00000001
        # R7 pointing to mini function (+1 thumb)
        r0=00000000, r7=20000011
        AIRCR FA050000
        ICSR  00400000
                ISRPENDING set
        ICPR  00018040
        DHCSR 0003000B
        DFSR  00000000
        DEMCR 00000001

        ----- After Resume -----
        # PC is pointing to debug_trampoline_end.
        # LR is pointing to the place where we return from 
        # our mini-program (i.e. the BKPT instruction) +1
        # for thumb.
        PC=000001AE, LR=000001AF, MSP=20000080
        XSPR  01000000
        CTL/PRIMASK  00000001
        # See the R0 is altered and R7 is still potinting 
        # to the mini-program
        r0=00000008, r7=20000011
        AIRCR FA050000
        ICSR  00400000
                ISRPENDING set
        ICPR  00018040
        DHCSR 0003000B
        # We got a BKPT, as expected
        DFSR  00000002
        DEMCR 00000001

Running a Flash Function Via the Debug Trampoline
=================================================

        ----- Before Resume -----
        PC=000001A8, LR=FFFFFFFF, MSP=20000080
        XSPR  01000000
        CTL/PRIMASK  00000001
        # R7 pointing to flash function.
        r0=00000000, r7=00002491
        AIRCR FA050000
        ICSR  00400000
                ISRPENDING set
        ICPR  00018040
        DHCSR 0003000B
        DFSR  00000000
        DEMCR 00000001

        ----- After Resume -----
        # PC is pointing to the debug_trampoline_end
        PC=000001AE, LR=0000249B, MSP=20000080
        # Bit 30 here: application status register, zero flag.
        XSPR  41000000
        CTL/PRIMASK  00000001
        r0=00000000, r7=00002491
        AIRCR FA050000
        ICSR  00400000
                ISRPENDING set
        ICPR  00018040
        # Halt/debug
        DHCSR 0003000B
        # BKPT as epected.
        DFSR  00000002
        DEMCR 00000001
