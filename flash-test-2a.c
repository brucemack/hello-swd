#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"

#define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))

void __no_inline_not_in_flash_func(move_to_flash)(const uint8_t* code, unsigned int code_len) {

    // This function transfers code into flash memory using only the bootrom functions described
    // in the RP2040 datasheet in section 2.8.3.1.3.
    //
    // This is a simulation of what will need to happen when this process is invoked by a 
    // debugger.

    // The steps here mirror what is done in the Pico SDK function 
    // called flash_range_erase().  Please see:
    // https://github.com/raspberrypi/pico-sdk/blob/develop/src/rp2_common/hardware_flash/flash.c#L114

    // Find the location of the key ROM functions
    void (*rom_debug_trampoline)() = rom_func_lookup(rom_table_code('D', 'T'));
    //printf("DT %X\n", rom_debug_trampoline);
    void (*rom_connect_internal_flash)() = rom_func_lookup(rom_table_code('I', 'F'));
    //printf("IF %X\n", rom_connect_internal_flash);
    void (*rom_flash_exit_xip)() = rom_func_lookup(rom_table_code('E', 'X'));
    //printf("EX %X\n", rom_flash_exit_xip);
    void (*rom_flash_range_erase)() = rom_func_lookup(rom_table_code('R', 'E'));
    //printf("RE %X\n", rom_flash_range_erase);
    void (*rom_flash_range_program)() = rom_func_lookup(rom_table_code('R', 'P'));
    //printf("RP %X\n", rom_flash_range_program);
    void (*rom_flash_flush_cache)() = rom_func_lookup(rom_table_code('F', 'C'));
    //printf("FC %X\n", rom_flash_flush_cache);

    // Seutp for serial-mode operations
    rom_connect_internal_flash();
    rom_flash_exit_xip();
    // Erase and program
    rom_flash_range_erase(0, code_len, 4096, 0xd8);
    rom_flash_range_program(0, code, code_len);
    rom_flash_flush_cache();

    // Force reset
    AIRCR_Register = 0x5FA0004;
}