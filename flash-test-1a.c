#include "pico/stdlib.h"
#include "hardware/flash.h"

#define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))

void __no_inline_not_in_flash_func(move_to_flash)(const uint8_t* code, unsigned int code_len) {

    // Erase flash pages
    flash_range_erase(0, code_len);

    // Copy new code 
    flash_range_program(0, code, code_len);

    // Force reset
    AIRCR_Register = 0x5FA0004;
}