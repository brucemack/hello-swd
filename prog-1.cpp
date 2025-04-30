/**
 * A demonstration program that flashes some firmware into an RP2040 via 
 * the SWD port.
 * 
 * Copyright (C) Bruce MacKinnon, 2025
 */
#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/flash.h"
#include "pico/bootrom.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"

// NOTE: These relate to the TARGET board, not the flashing board.
#include "blinky-bin-rp2040.h"
//#include "blinky-bin-rp2350.h"

#include "kc1fsz-tools/SWDUtils.h"
#include "kc1fsz-tools/rp2040/SWDDriver.h"

using namespace kc1fsz;

const uint LED_PIN = 25;

//#define CLK_PIN (2)
//#define DIO_PIN (3)
#define CLK_PIN (16)
#define DIO_PIN (17)

void display_status(SWDDriver& swd) {

    uint32_t pc = 0;
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x0000000f); r != 0)
        return;
    if (swd.pollREGRDY() != 0)
        return;
    if (const auto r = swd.readWordViaAP(ARM_DCRDR); !r.has_value()) {
        return;
    } else {
        pc = *r;
    }

    uint32_t lr = 0;
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x0000000e); r != 0)
        return;
    if (swd.pollREGRDY() != 0)
        return;
    if (const auto r = swd.readWordViaAP(ARM_DCRDR); !r.has_value()) {
        return;
    } else {
        lr = *r;
    }

    uint32_t msp = 0;
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0b10001); r != 0)
        return;
    if (swd.pollREGRDY() != 0)
        return;
    if (const auto r = swd.readWordViaAP(ARM_DCRDR); !r.has_value()) {
        return;
    } else {
        msp = *r;
    }

    printf("PC=%08X, LR=%08X, MSP=%08X\n", pc, lr, msp);

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
    // Read (combined)
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0b10100); r != 0)
        return;
    if (swd.pollREGRDY() != 0)
        return;
    if (const auto r = swd.readWordViaAP(ARM_DCRDR); !r.has_value()) {
        return;
    } else {
        printf("CTL/PRIMASK  %08X\n", *r);
    }

    uint32_t r0 = 0;
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00000000); r != 0)
        return;
    if (swd.pollREGRDY() != 0)
        return;
    if (const auto r = swd.readWordViaAP(ARM_DCRDR); !r.has_value()) {
        return;
    } else {
        r0 = *r;
    }
    uint32_t r7 = 0;
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x00000007); r != 0)
        return;
    if (swd.pollREGRDY() != 0)
        return;
    if (const auto r = swd.readWordViaAP(ARM_DCRDR); !r.has_value()) {
        return;
    } else {
        r7 = *r;
    }

    printf("r0=%08X, r7=%08X\n", r0, r7);

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

    if (const auto r = swd.readWordViaAP(0xE000EDFC); !r.has_value()) {
        return;
    }
    else {
        printf("DEMCR %08X\n", *r);
    }   
}

int prog_1() {

    SWDDriver swd(CLK_PIN, DIO_PIN);

    swd.init();
    if (swd.connect()) 
        return -1;

    printf("Connect is good with APID %08X\n", swd.getAPID());
   
    if (const int rc = reset_into_debug(swd); rc != 0) {
        return -200 + rc;
    }
    
    if (const int rc = flash_and_verify(swd, 0, blinky_bin, blinky_bin_len); rc != 0) {
        printf("Flashed failed\n");
        return -100 + rc;
    }

    if (const int rc = reset(swd); rc != 0) {
        return -300 + rc;        
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

    printf("Flash Programming Demonstration 1\n");

    int rc = prog_1();
    if (rc != 0)
        printf("Programming failed %d\n", rc);
    else 
        printf("Programming succeeded\n");

    while (true) {        
    }
}