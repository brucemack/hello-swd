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

int flash(SWD& swd) {

    // DP SELECT - Set AP and DP bank 0
    if (const auto r = swd.writeDP(0x8, 0x00000000); r != 0)
        return -1;

    // Enable debug mode and halt
    if (const auto r = swd.writeWordViaAP(0xe000edf0, 0xa05f0003); r != 0)
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
    uint16_t rom_debug_trampoline_func = 0;

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
        else if (func_code == rom_table_code('D', 'T'))
            rom_debug_trampoline_func = func_addr;
        
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
    void* buf = malloc(whole_pages * page_size); 
    memset(buf, 0, whole_pages * page_size);
    memcpy(buf, blinky_bin, blinky_bin_len);

    unsigned int a = 0x20000100;     

    // Copy the entire program into RAM (max 4x64k)
    printf("Program bytes %u, words %u\n", (whole_pages * page_size), (whole_pages * page_size) / 4);
    if (const auto r = swd.writeMultiWordViaAP(0x20000100, 
        (const uint32_t*)buf, (whole_pages * page_size) / 4); r != 0)
        return -1;

    /*
    rom_connect_internal_flash();
    rom_flash_exit_xip();
    // Erase and program
    rom_flash_range_erase(0, code_len, 4096, 0xd8);
    rom_flash_range_program(0, code, code_len);
    rom_flash_flush_cache();
    */

    // Write the target function address in the r7 register (+1 for thumb)
    if (const auto r = swd.writeWordViaAP(ARM_DCRDR, 0x20000011); r != 0)
        return -1;
    // [16] is 1 (write), [6:0] 0b0000111 means r7
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00010007); r != 0) 
        return -1;
    // Poll to find out if the write is done
    if (swd.pollREGRDY() != 0)
        return -1;

    // Clear r0
    if (const auto r = swd.writeWordViaAP(ARM_DCRDR, 0x00000000); r != 0)
        return -1;
    // [16] is 1 (write), [6:0] 0b0000000 means r0
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00010000); r != 0) 
        return -1;
    // Poll to find out if the write is done
    if (swd.pollREGRDY() != 0)
        return -1;

    // Write a value in the MSP register
    if (const auto r = swd.writeWordViaAP(ARM_DCRDR, 0x20000080); r != 0)
        return -1;
    // [16] is 1 (write), [6:0] 0b0001101 means MSP
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x0001000d); r != 0) 
        return -1;
    // Poll to find out if the write is done
    if (swd.pollREGRDY() != 0)
        return -1;

    // Write a value into the debug return address value. Note a+1 for 
    // thumb mode.
    if (const auto r = swd.writeWordViaAP(ARM_DCRDR, rom_debug_trampoline_func); r != 0)
        return -1;
    // [16] is 1 (write), [6:0] 0b0001111 
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x0001000f); r != 0) 
        return -1;
    // Poll to find out if the write is done
    if (swd.pollREGRDY() != 0)
        return -1;

    // Leave halt mode
    if (const auto r = swd.writeWordViaAP(0xe000edf0, 0xa05f0000); r != 0) {
        printf("Fail10 %d\n", r);
        return -1;
    }

    // We should immediately hit a breakpoint here

    // Re-enter debug mode
    if (const auto r = swd.writeWordViaAP(0xe000edf0, 0xa05f0003); r != 0) {
        printf("Fail10 %d\n", r);
        return -1;
    }

    // Read back from the r0 register
    // [16] is 0 (read), [6:0] 0b0000000 means r0
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00000000); r != 0)
        return -1;
    // Poll to find out if the read is done
    if (swd.pollREGRDY() != 0)
        return -1;
    if (const auto r = swd.readWordViaAP(ARM_DCRDR); !r.has_value()) {
        printf("Fai12 %d\n", r.error());
        return -1;
    } else {
        printf("r0: %X\n", *r);
    }

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

    // Enable debug mode and halt
    if (const auto r = swd.writeWordViaAP(0xe000edf0, 0xa05f0003); r != 0) {
        printf("Fail10 %d\n", r);
    }

    if (const auto r = swd.readWordViaAP(0xe000ed0c); !r.has_value()) {
        printf("Fai12 %d\n", r.error());
        return -1;
    } else {
        printf("NVIC.AIRCR: %X\n", *r);
    }

    if (const auto r = swd.readWordViaAP(0xe000edf0); !r.has_value()) {
        printf("Fai12 %d\n", r.error());
        return -1;
    } else {
        printf("DHCSR: %X\n", *r);
    }

    /*
    unsigned int a = 0x000001a8;     
    if (const auto r = swd.readWordViaAP(a); !r.has_value()) {
        printf("Fai12 %d\n", r.error());
        return -1;
    } else {
        printf("At %08X = %08X\n", a, *r);
    }
    */

    flash(swd);

    // Leave debug
    if (const auto r = swd.writeWordViaAP(0xe000edf0, 0xa05f0000); r != 0) {
        printf("Fail10 %d\n", r);
    }

    // Trigger a reset by writing SYSRESETREQ to NVIC.AIRCR.
    //printf("Resetting ...\n");
    //if (const auto r = swd.writeWordViaAP(0xe000ed0c, 0x05fa0004); r != 0) {
    //    printf("Fail10 %d\n", r);
    //}

    // Finalize any last writes
    swd.writeBitPattern("00000000");

    printf("Blocking\n");
    
    while (true) {        
    }
}
