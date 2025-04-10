# ARM Thumb instructions needed to call a ROM function 
# on an RP2040.
#
# arm-none-eabi-as --warn --fatal-warnings -mcpu=cortex-m0plus -g ../caller.s -o caller.obj
# arm-none-eabi-objdump -S caller.obj 
# arm-none-eabi-objcopy -O binary caller.obj caller.bin
# xxd -g4 -i caller.bin >> caller-bin.h

    .cpu cortex-m0plus
    .thumb
    .section .text
    .align 2
    .thumb_func
    .global start_point
start_point:  
    ldr r4, debug_trampoline_addr
    ldr r7, target_func_addr
    ldr r0, param0
    ldr r1, param1
    ldr r2, param2
    ldr r3, param3
# Call the trampoline
    blx r4
    .align 4

# Here is where the values need to be plugged in:

# The address of the trampoline ROM function (looked up using "DT").
debug_trampoline_addr:
    .word 0x00000000

# The address of the function being called. 
target_func_addr:
    .word 0x00000000

# Parameters 0, 1, 2, 3 being passed to the target function.
param0:
    .word 0x00000000
param1:
    .word 0x00000000
param2:
    .word 0x00000000
param3:
    .word 0x00000000
