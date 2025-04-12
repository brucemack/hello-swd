#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include "SWD.h"

using namespace kc1fsz;

const uint LED_PIN = 25;

#define CLK_PIN (2)
#define DIO_PIN (3)


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

    printf("Start\n");

    SWD swd;

    swd.init();

    // Long period with DIO=1, CLK=0
    swd.setDIO(true);
    sleep_us(1655);
    swd.writeBitPattern("11111111");

    sleep_us(1);

    // JTAG-TO-DS Conversion
    swd.writeJTAGToDSConversion();

    sleep_us(1);

    // Delay with CLK=0/DIO=0
    swd.setDIO(false);
    sleep_us(160);
    // 8 clocks with DIO=1
    swd.writeBitPattern("11111111");
    sleep_us(1);
    // Selection alert (128 bits)
    swd.writeSelectionAlert();   
    // ARM Coresight activation code
    swd.writeActivaionCode();
    // Line reset
    swd.writeLineReset();

    swd.writeBitPattern("00000000");
    // Delay with CLK=0/DIO=0
    swd.setDIO(false);
    sleep_us(150);

    // DP TARGETSEL, DP for Core 0.  We ignore the ACK here
    if (const auto r = swd.writeDP(0b1100, 0x01002927, true); r != 0) {
        printf("DP TARGETSEL write failed %d\n", r);
        return -1;
    }

    // Delay with CLK=0/DIO=0
    swd.setDIO(false);
    sleep_us(732);

    // Read the ID code
    if (const auto r = swd.readDP(0b0000); r.has_value())
      printf("IDCODE Value %X\n", *r);
    else {
        printf("ERROR %d\n", r.error());
        return -1;
    }

    // FOLLOWING PICOREG

    // Abort
    if (const auto r = swd.writeDP(0b0000, 0x0000001e); r != 0) {
        printf("Fail1 %d\n", r);
        return -1;
    }

    // Set AP and DP bank 0
    if (const auto r = swd.writeDP(0b1000, 0x00000000); r != 0) {
        printf("Fail2 %d\n", r);
        return -1;
    }

    // Power up
    if (const auto r = swd.writeDP(0b0100, 0x50000001); r != 0) {
        printf("Fail3 %d\n", r);
        return -1;
    }

    // Read DP CTRLSEL
    if (const auto r = swd.readDP(0b0100); !r.has_value()) {
        printf("Fail4 %d\n", r.error());
        return -1;
    } else {
        printf("Status %X\n", *r);
        if ((*r & 0x80000000) && (*r & 0x20000000))
            printf("Power up is good\n");
    }

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

    // DP SELECT - Set AP and DP bank 0
    if (const auto r = swd.writeDP(0x8, 0x00000000); r != 0) {
        printf("Fail9 %d\n", r);
        return -1;
    }
    
    // Enable debug mode
    if (const auto r = swd.writeWordViaAP(0xe000edf0, 0xa05f0003); r != 0) {
    //if (const auto r = swd.writeWordViaAP(0xe000edf0, 0xa05f0001); r != 0) {
    //if (const auto r = swd.writeWordViaAP(0xe000edf0, 0xa05f0000); r != 0) {
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

    // Write a value into the LR register
    // [16] is 1 (write), [6:0] 0b0001111 means LR
    // NOTICE: The DCRDR register has to be written first!
    if (const auto r = swd.writeWordViaAP(ARM_DCRDR, 0x6); r != 0)
        return -1;
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x0001000F); r != 0) 
        return -1;
    // Poll to find out if the write is done
    if (swd.pollREGRDY() != 0)
        return -1;

    // Read back from the LR register
    // [16] is 0 (read), [6:0] 0b0001111 means LR
    if (const auto r = swd.writeWordViaAP(ARM_DCRSR, 0x0000000f); r != 0)
        return -1;
    // Poll to find out if the read is done
    if (swd.pollREGRDY() != 0)
        return -1;
    if (const auto r = swd.readWordViaAP(ARM_DCRDR); !r.has_value()) {
        printf("Fai12 %d\n", r.error());
        return -1;
    } else {
        printf("LR: %X\n", *r);
    }

    // Trigger a reset by writing SYSRESETREQ to NVIC.AIRCR.
    //printf("Resetting ...\n");
    //if (const auto r = swd.writeWordViaAP(0xe000ed0c, 0x05fa0004); r != 0) {
    //    printf("Fail10 %d\n", r);
    //}
    
    while (true) {        
    }
}
