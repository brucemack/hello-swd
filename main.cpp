#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include "SWD.h"

using namespace kc1fsz;

const uint LED_PIN = 25;

int main(int, const char**) {

    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
        
    gpio_put(LED_PIN, 1);
    sleep_ms(500);
    gpio_put(LED_PIN, 0);
    sleep_ms(500);

    printf("Start\n");

    SWD swd;

    swd.init();

    swd.writeLineReset();
    swd.writeBitPattern("11111111");

    printf("A\n");

    // JTAG-TO-DS Conversion
    swd.writeJTAGToDSConversion();
    // Delay with CLK=0/DIO=0
    swd.setDIO(false);
    sleep_us(160);
    // 8 clocks with DIO=1
    swd.writeBitPattern("11111111");
    // Selection alert
    swd.writeSelectionAlert();
    // ARM Coresight activation code
    swd.writeActivaionCode();
    // Line reset
    swd.writeLineReset();
    swd.writeBitPattern("00000000");
    // Delay with CLK=0/DIO=0
    swd.setDIO(false);
    sleep_us(150);
    // DP TARGETSEL, DP for Core 0
    swd.writeDP(0b11, 0x01002927);
    // Delay with CLK=0/DIO=0
    swd.setDIO(false);
    sleep_us(732);
    // Read the ID code
    if (const auto r = swd.readDP(0b00); r.has_value())
        printf("Value %X\n", *r);
    else
        printf("ERROR %d\n", r.error());
    
    while (true) {        
    }
}
