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
    if (const auto r = swd.writeDP(0b11, 0x01002927, true); r != 0) {
        printf("DP TARGETSEL write failed %d\n", r);
    }
    else {
        // Delay with CLK=0/DIO=0
        swd.setDIO(false);
        sleep_us(732);
        // Read the ID code
        if (const auto r = swd.readDP(0b00); r.has_value())
            printf("Value %X\n", *r);
        else
            printf("ERROR %d\n", r.error());
    }

    while (true) {        
    }
}
