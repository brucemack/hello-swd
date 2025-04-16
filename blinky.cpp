/*
# Make sure that the PICO_SDK_PATH is set properly

cd /home/bruce/pico/hello-swd
# You need main.c and CMakeLists.txt
cp ../pico-sdk/external/pico_sdk_import.cmake .
mkdir build
cd build
cmake -DPICO_BOARD=pico ..
make

# Make sure the SWD is connected properly:
# GPIO24 (Pin 18) to SWDIO
# GPIO25 (Pin 22) to SWCLK
# GND (Pin 20) to SWGND

# Use this command to flash:
openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program blinky.elf verify reset exit"

# Or this, when using (a) local build of openocd (b) Pico debug probe (c) Pico2
# ~/git/openocd/src/openocd -s ~/git/openocd/tcl -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "rp2350.dap.core1 cortex_m reset_config sysresetreq" -c "program blinky.elf verify reset exit"

# Looking at the serial port (Pico 1):
minicom -b 115200 -o -D /dev/ttyACM0

# Looking at the serial port (Pico 2):
minicom -b 115200 -o -D /dev/ttyACM1
*/
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define LED_PIN (25)

int main() {
 
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
        
    gpio_put(LED_PIN, 1);
    sleep_ms(500);
    gpio_put(LED_PIN, 0);
    sleep_ms(500);
    
    printf("Hello World\n");

    unsigned int i = 0;

    while (1) {
      gpio_put(LED_PIN, 0);
      sleep_ms(500);
      gpio_put(LED_PIN, 1);
      sleep_ms(500);
      printf("Loop 2 %u\n", i++);
    }

}

