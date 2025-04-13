# ARM Thumb instructions needed to call a function 
# on an RP2040.
#
# arm-none-eabi-as --warn --fatal-warnings -mcpu=cortex-m0plus -g ../caller.s -o caller.obj
# arm-none-eabi-objdump -S caller.obj 
# arm-none-eabi-objcopy -O binary caller.obj caller.bin
# xxd -g4 -i caller.bin > ../caller-bin.h
    .cpu cortex-m0plus
    .thumb
    .section .text
    .align 2
    .thumb_func
    .global start_point
start_point:  
# Make sure that the LSB is set (i.e. thumb mode)
    mov r6, #1
    orr r7, r7, r6
    blx r7
# Halt on return     
    bkpt #0
    b start_point

