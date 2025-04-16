#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/flash.h"
#include "pico/bootrom.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include "blinky-bin.h"
#include "SWD.h"

using namespace kc1fsz;

const uint LED_PIN = 25;

#define CLK_PIN (2)
#define DIO_PIN (3)

static uint32_t rom_table_code(char c1, char c2) {
  return (c2 << 8) | c1;
}

void display_status(SWD& swd) {

    // Read PC
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x0000000f); r != 0)
        return;
    if (swd.pollREGRDY() != 0)
        return;
    if (const auto r = swd.readWordViaAP(ARM_DCRDR); !r.has_value()) {
        return;
    } else {
        printf("PC    %08X\n", *r);
    }
    // Read LR
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x0000000e); r != 0)
        return;
    if (swd.pollREGRDY() != 0)
        return;
    if (const auto r = swd.readWordViaAP(ARM_DCRDR); !r.has_value()) {
        return;
    } else {
        printf("LR    %08X\n", *r);
    }
    // Read XPSR
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00000010); r != 0)
        return;
    if (swd.pollREGRDY() != 0)
        return;
    if (const auto r = swd.readWordViaAP(ARM_DCRDR); !r.has_value()) {
        return;
    } else {
        printf("XSPR  %08X\n", *r);
    }
    // Read R0
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00000000); r != 0)
        return;
    if (swd.pollREGRDY() != 0)
        return;
    if (const auto r = swd.readWordViaAP(ARM_DCRDR); !r.has_value()) {
        return;
    } else {
        printf("r0    %08X\n", *r);
    }

    if (const auto r = swd.readWordViaAP(0xe000ed0c); !r.has_value()) {
        return;
    } else {
        printf("AIRCR %08X\n", *r);
    }

    if (const auto r = swd.readWordViaAP(0xE000ED04); !r.has_value()) {
        return;
    } else {
        printf("ICSR  %08X\n", *r);
        if (*r & 0x00400000)
            printf("  ISRPENDING set\n");
    }

    if (const auto r = swd.readWordViaAP(0xE000E280); !r.has_value()) {
        return;
    } else {
        printf("ICPR  %08X\n", *r);
    }

    if (const auto r = swd.readWordViaAP(0xe000edf0); !r.has_value()) {
        return;
    } else {
        printf("DHCSR %08X\n", *r);
    }

    if (const auto r = swd.readWordViaAP(0xE000ED30); !r.has_value()) {
        return;
    }
    else {
        printf("DFSR  %08X\n", *r);
    }
}

/**
 * IMPORTANT: We are assuming the target is already in debug/halt mode 
 * when this function is called.  On exit, this target will still be 
 * in debug/halt mode.
 */
std::expected<uint32_t, int> call_function(SWD& swd, 
    uint32_t trampoline_addr, uint32_t func_addr, 
    uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {

    printf("Calling %08X ( %08X, %08X, %08X, %08X)\n", func_addr, a0, a1, a2, a3);

    // Write the target function address in the r7 register
    if (const auto r = swd.writeWordViaAP(ARM_DCRDR, func_addr); r != 0)
        return std::unexpected(-1);
    // [16] is 1 (write), [6:0] 0b0000111 means r7
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00010007); r != 0) 
        return std::unexpected(-2);
    // Poll to find out if the write is done
    if (swd.pollREGRDY() != 0)
        return std::unexpected(-3);

    // Write the argumens in the r0-r3 registers

    if (const auto r = swd.writeWordViaAP(ARM_DCRDR, a0); r != 0)
        return std::unexpected(-4);
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00010000); r != 0) 
        return std::unexpected(-5);
    if (swd.pollREGRDY() != 0)
        return std::unexpected(-6);

    if (const auto r = swd.writeWordViaAP(ARM_DCRDR, a1); r != 0)
        return std::unexpected(-4);
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00010001); r != 0) 
        return std::unexpected(-5);
    if (swd.pollREGRDY() != 0)
        return std::unexpected(-6);

    if (const auto r = swd.writeWordViaAP(ARM_DCRDR, a2); r != 0)
        return std::unexpected(-4);
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00010002); r != 0) 
        return std::unexpected(-5);
    if (swd.pollREGRDY() != 0)
        return std::unexpected(-6);

    if (const auto r = swd.writeWordViaAP(ARM_DCRDR, a3); r != 0)
        return std::unexpected(-4);
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00010003); r != 0) 
        return std::unexpected(-5);
    if (swd.pollREGRDY() != 0)
        return std::unexpected(-6);

    // Write a value in the MSP register
    if (const auto r = swd.writeWordViaAP(ARM_DCRDR, 0x20000080); r != 0)
        return std::unexpected(-7);
    // [16] is 1 (write), [6:0] 0b0001101 means MSP
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x0001000d); r != 0) 
        return std::unexpected(-8);
    // Poll to find out if the write is done
    if (swd.pollREGRDY() != 0)
        return std::unexpected(-9);

    // Set xPSR to 0x01000000 (ESPR.T=1).
    // NOTE: I DON'T UNDERSTANT THIS
    if (const auto r = swd.writeWordViaAP(ARM_DCRDR, 0x01000000); r != 0)
        return std::unexpected(-10);
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00010010); r != 0) 
        return std::unexpected(-11);
    // Poll to find out if the write is done
    if (swd.pollREGRDY() != 0)
        return std::unexpected(-12);

    // Write a value into the debug return address value. 
    if (const auto r = swd.writeWordViaAP(ARM_DCRDR, trampoline_addr); r != 0)
        return std::unexpected(-10);
    // [16] is 1 (write), [6:0] 0b0001111 
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x0001000f); r != 0) 
        return std::unexpected(-11);
    // Poll to find out if the write is done
    if (swd.pollREGRDY() != 0)
        return std::unexpected(-12);

    // Mask interrupts
    if (const auto r = swd.writeWordViaAP(ARM_DCRDR, 0x00000001); r != 0)
        return std::unexpected(-10);
    // [16] is 1 (write), [4:0] 0b010100 
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00010014); r != 0) 
        return std::unexpected(-11);
    // Poll to find out if the write is done
    if (swd.pollREGRDY() != 0)
        return std::unexpected(-12);

    // Clear pending interrupts
    if (const auto r = swd.writeWordViaAP(0xE000E280, 0xffffffff); r != 0)
        return std::unexpected(-13);

    // Clear DFSR
    uint32_t dfsr = 0;
    if (const auto r = swd.readWordViaAP(0xE000ED30); !r.has_value()) {
        return std::unexpected(-17);
    }
    else {
        dfsr = *r;
    }
    if (const auto r = swd.writeWordViaAP(0xE000ED30, dfsr); r != 0)
        return std::unexpected(-13);

    printf("Before Status\n");
    display_status(swd);

    // Leave halt mode so that things can bappen
    if (const auto r = swd.writeWordViaAP(0xe000edf0, 0xa05f0000 | 0b1000); r != 0)
        return std::unexpected(-13);

    unsigned int j = 0;
    while (true) {

        // Enter debug
        if (const auto r = swd.writeWordViaAP(0xe000edf0, 0xa05f0003 | 0b1000); r != 0)
            return std::unexpected(-13);

        printf("After Status [2]\n");
        display_status(swd);

        sleep_ms(30000);

        // Leave halt mode so that things can bappen
        if (const auto r = swd.writeWordViaAP(0xe000edf0, 0xa05f0000 | 0b1000); r != 0)
            return std::unexpected(-13);
    }

    // We should immediately hit a breakpoint here

    // Re-enter debug mode
    if (const auto r = swd.writeWordViaAP(0xe000edf0, 0xa05f0003); r != 0)
        return std::unexpected(-14);

    // Read back from the r0 register
    // [16] is 0 (read), [6:0] 0b0000000 means r0
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00000000); r != 0)
        return std::unexpected(-15);
    // Poll to find out if the read is done
    if (swd.pollREGRDY() != 0)
        return std::unexpected(-16);
    if (const auto r = swd.readWordViaAP(ARM_DCRDR); !r.has_value()) {
        return std::unexpected(-17);
    } else {
        return *r;
    }
}


int flash(SWD& swd) {

    // DP SELECT - Set AP and DP bank 0
    if (const auto r = swd.writeDP(0x8, 0x00000000); r != 0)
        return -1;

    // ----- Get the ROM function locations -----------------------------------

    // Observation from an RP2040: table pointer is a 0x7a. This is interesting
    // because it is not a multiple of 4, so we need to be careful when searching
    // the lookup table - use half-word accesses.

    // Get the start of the ROM function table
    uint16_t tab_ptr;
    if (const auto r = swd.readHalfWordViaAP(0x00000014); !r.has_value()) {
        return -3;
    } else {
        tab_ptr = *r;
    }

    uint16_t rom_connect_internal_flash_func = 0;
    uint16_t rom_flash_exit_xip_func = 0;
    uint16_t rom_flash_range_erase_func = 0;
    uint16_t rom_flash_range_program_func = 0;
    uint16_t rom_flash_flush_cache_func = 0;
    uint16_t rom_flash_enter_cmd_xip_func = 0;
    uint16_t rom_debug_trampoline_func = 0;
    uint16_t rom_debug_trampoline_end_func = 0;

    // Iterate through the table until we find a null function code
    // Each entry word is two 8-bit codes and a 16-bit 
    // address.  

    while (true) {

        uint16_t func_code;
        if (const auto r = swd.readHalfWordViaAP(tab_ptr); !r.has_value()) {
            return -4;
        } else {
            func_code = *r;
        }

        uint16_t func_addr;
        if (const auto r = swd.readHalfWordViaAP(tab_ptr + 2); !r.has_value()) {
            return -5;
        } else {
            func_addr = *r;
        }

        if (func_code == 0)
            break;

        if (func_code == rom_table_code('I', 'F'))
            rom_connect_internal_flash_func = func_addr;
        else if (func_code == rom_table_code('E', 'X'))
            rom_flash_exit_xip_func = func_addr;
        else if (func_code == rom_table_code('R', 'E'))
            rom_flash_range_erase_func = func_addr;
        else if (func_code == rom_table_code('R', 'P'))
            rom_flash_range_program_func = func_addr;
        else if (func_code == rom_table_code('F', 'C'))
            rom_flash_flush_cache_func = func_addr;
        else if (func_code == rom_table_code('C', 'X'))
            rom_flash_enter_cmd_xip_func = func_addr;
        else if (func_code == rom_table_code('D', 'T'))
            rom_debug_trampoline_func = func_addr;
        else if (func_code == rom_table_code('D', 'E')) 
            rom_debug_trampoline_end_func = func_addr;
        
        tab_ptr += 4;
    }

    /* NOT NEED ANYMORE!
    // Load the trampoline program into memory.  We're doing this because 
    // the official _debug_trampoline() doesn't seem to work.
    if (const auto r = swd.writeWordViaAP(0x20000000, 0x43372601); r != 0)
        return -1;
    if (const auto r = swd.writeWordViaAP(0x20000004, 0xbe0047b8); r != 0)
        return -1;
    if (const auto r = swd.writeWordViaAP(0x20000008, 0x46c0e7fa); r != 0)
        return -1;
    */

    // Load a small test program that sets r0 and returns.  Note the starting
    // location
    if (const auto r = swd.writeWordViaAP(0x20000010, 0x47702008); r != 0)
        return -1;
    
    // Take size of binary and pad it up to a 4K boundary
    unsigned int page_size = 4096;
    unsigned int whole_pages = blinky_bin_len / page_size;
    unsigned int remainder = blinky_bin_len % page_size;
    // Make sure we are using full pages
    if (remainder)
        whole_pages++;
    // Make a zero buffer large enough and copy in the code
    unsigned int buf_len = whole_pages * page_size;
    void* buf = malloc(buf_len); 
    memset(buf, 0, buf_len);
    memcpy(buf, blinky_bin, blinky_bin_len);

    unsigned int a = 0x20000100;     

    // Copy the entire program into RAM (max 4x64k)
    printf("Program bytes %u, words %u\n", buf_len, buf_len / 4);
    if (const auto r = swd.writeMultiWordViaAP(a, (const uint32_t*)buf, buf_len / 4); r != 0)
        return -1;

    if (const auto r = swd.readWordViaAP(a); !r.has_value()) {
        printf("Fai12 %d\n", r.error());
        return -1;
    } else {
        printf("RAM At %08X = %08X\n", a, *r);
    }

    // These are the functions that need to be called:
    //
    // 0. connect_internal_flash();
    // 1. flash_exit_xip();
    // 2. flash_range_erase(0, code_len, 4096, 0xd8);
    // 3. flash_range_program(0, code, code_len);
    // 4. flash_flush_cache();

    //if (const auto r = call_function(swd, rom_debug_trampoline_func, rom_connect_internal_flash_func, 0, 0, 0, 0); !r.has_value())
    if (const auto r = call_function(swd, rom_debug_trampoline_func, 0x20000011, 0, 0, 0, 0); !r.has_value())
        return -1;
       
    /*
    if (const auto r = call_function(swd, rom_debug_trampoline_func, rom_flash_exit_xip_func, 0, 0, 0, 0); !r.has_value())
        return -1;
    if (const auto r = call_function(swd, rom_debug_trampoline_func, rom_flash_range_erase_func, 0, buf_len, 1 << 16, 0xd8); !r.has_value())
        return -1;


    if (const auto r = call_function(swd, rom_debug_trampoline_func, rom_connect_internal_flash_func, 0, 0, 0, 0); !r.has_value())
        return -1;
    if (const auto r = call_function(swd, rom_debug_trampoline_func, rom_flash_exit_xip_func, 0, 0, 0, 0); !r.has_value())
        return -1;
    if (const auto r = call_function(swd, rom_debug_trampoline_func, rom_flash_range_program_func, 0, a, buf_len, 0); !r.has_value())
        return -1;
    if (const auto r = call_function(swd, rom_debug_trampoline_func, rom_flash_flush_cache_func, 0, 0, 0, 0); !r.has_value())
        return -1;

    if (const auto r = call_function(swd, rom_debug_trampoline_func, rom_flash_flush_cache_func, 0, 0, 0, 0); !r.has_value())
        return -1;
    if (const auto r = call_function(swd, rom_debug_trampoline_func, rom_flash_enter_cmd_xip_func, 0, 0, 0, 0); !r.has_value())
        return -1;

    a = 0x10000000;
    if (const auto r = swd.readWordViaAP(a); !r.has_value()) {
        printf("Fai12 %d\n", r.error());
        return -1;
    } else {
        printf("Post flash at %08X = %08X\n", a, *r);
    }
    */

    return 0;
}

int main(int, const char**) {

    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(CLK_PIN);
    gpio_set_dir(CLK_PIN, GPIO_OUT);        
    gpio_put(CLK_PIN, 0);
    gpio_init(DIO_PIN);
    gpio_set_dir(DIO_PIN, GPIO_OUT);        
    gpio_put(DIO_PIN, 0);
       
    gpio_put(LED_PIN, 1);
    sleep_ms(500);
    gpio_put(LED_PIN, 0);
    sleep_ms(500);

    printf("Start 2\n");

    SWD swd;

    swd.init();
    if (swd.connect()) 
        return -1;
   
    // DP SELECT - Set AP bank F, DP bank 0
    // [31:24] AP Select
    // [7:4]   AP Bank Select (the active four-word bank)
    if (const auto r = swd.writeDP(0b1000, 0x000000f0); r != 0) {
        printf("Fail5 %d\n", r);
        return -1;
    }

    // Read AP addr 0xFC. [7:4] bank address set previously, [3:0] set here.
    // 0xFC is the AP identification register.
    // The actual data comes back during the DP RDBUFF read
    if (const auto r = swd.readAP(0x0c); !r.has_value()) {
        printf("Fail6 %d\n", r.error());
        return -1;
    }

    // DP RDBUFF - Read AP result
    if (const auto r = swd.readDP(0xc); !r.has_value()) {
        printf("Fail7 %d\n", r.error());
        return -1;
    } else {
        printf("AP ID %X\n", *r);

    }

    // DP SELECT - Set AP and DP bank 0
    if (const auto r = swd.writeDP(0x8, 0x00000000); r != 0) {
        printf("Fail9 %d\n", r);
        return -1;
    }

    // Write to the AP Control/Status Word (CSW), auto-increment, word values
    //
    // 1010_0010_0000_0000_0001_0010
    // 
    // [5:4] 01  : Auto Increment set to "Increment Single," which increments by the size of the access.
    // [2:0] 010 : Size of the access to perform, which is 32 bits in this case. 
    //if (const auto r = swd.writeAP(0b0000, 0xa2000012); r != 0) {
    if (const auto r = swd.writeAP(0b0000, 0x22000012); r != 0) {
        printf("Fail8 %d\n", r);
        return -1;
    }

    // ----- RESET ------------------------------------------------------------

    // Enable debug mode and halt
    if (const auto r = swd.writeWordViaAP(0xe000edf0, 0xa05f0003 | 0b1000); r != 0) 
        return -1;

    // Set DEMCR.VC_CORERESET=1 so that we come up in debug mode after reset
    if (const auto r = swd.writeWordViaAP(0xE000EDFC, 0x00000001); r != 0)
        return -1;

    // Trigger a reset by writing SYSRESETREQ to NVIC.AIRCR.
    if (const auto r = swd.writeWordViaAP(0xe000ed0c, 0x05fa0004); r != 0)
        return -1;

    // ???
    sleep_ms(10);

    // DP SELECT - Set AP and DP bank 0
    if (const auto r = swd.writeDP(0x8, 0x00000000); r != 0) 
        return -1;

    // Write to the AP Control/Status Word (CSW), auto-increment, word values
    //
    // 1010_0010_0000_0000_0001_0010
    // 
    // [5:4] 01  : Auto Increment set to "Increment Single," which increments by the size of the access.
    // [2:0] 010 : Size of the access to perform, which is 32 bits in this case. 
    if (const auto r = swd.writeAP(0b0000, 0x22000012); r != 0)
        return -1;

    // Move VTOR to SRAM
    if (const auto r = swd.writeWordViaAP(0xe000ed08, 0x20000000); r != 0)
        return -1;

    /*    
    unsigned int a = 0x10000000;     
    if (const auto r = swd.readWordViaAP(a); !r.has_value()) {
        printf("Fai12 %d\n", r.error());
        return -1;
    } else {
        printf("ROM At %08X = %08X\n", a, *r);
    }
    a += 4;
    if (const auto r = swd.readWordViaAP(a); !r.has_value()) {
        printf("Fai12 %d\n", r.error());
        return -1;
    } else {
        printf("ROM At %08X = %08X\n", a, *r);
    }
    */

    flash(swd);

    // Finalize any last writes
    swd.writeBitPattern("00000000");

    // Leave debug
    if (const auto r = swd.writeWordViaAP(0xe000edf0, 0xa05f0000); r != 0) {
        printf("Fail10 %d\n", r);
    }

    // Trigger a reset by writing SYSRESETREQ to NVIC.AIRCR.
    printf("Resetting ...\n");
    if (const auto r = swd.writeWordViaAP(0xe000ed0c, 0x05fa0004); r != 0) {
        printf("Fail10 %d\n", r);
    }

    // Finalize any last writes
    swd.writeBitPattern("00000000");

    printf("Blocking\n");
    
    while (true) {        
    }
}
