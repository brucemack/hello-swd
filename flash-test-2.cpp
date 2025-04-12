#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "blinky-bin.h"

#define LED_PIN (25)

extern "C" {
    void __no_inline_not_in_flash_func(move_to_flash)(const uint8_t* code, unsigned int code_len);
}

int main() {
 
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
        
    for (unsigned int i = 0; i < 4; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(250);
        gpio_put(LED_PIN, 0);
        sleep_ms(250);
    }
    
    printf("Flash Test 2\n");

    // Take size of binary and pad it up to a 4K boundary
    unsigned int page_size = 4096;
    unsigned int whole_pages = blinky_bin_len / page_size;
    unsigned int remainder = blinky_bin_len % page_size;
    // Make sure we are using full pages
    if (remainder)
        whole_pages++;
    // Make a zero buffer large enough and copy in the code
    void* buf = calloc(whole_pages * page_size, 1); 
    memcpy(buf, blinky_bin, blinky_bin_len);
    
    // Call out to a special RAM function that will do the actual
    // copy to flash. This is done in RAM since XIP reading will
    // become disabled while programming is happening.
    move_to_flash((const uint8_t*)buf, whole_pages * page_size);

    unsigned int i = 0;

    while (1) {
      gpio_put(LED_PIN, 0);
      sleep_ms(500);
      gpio_put(LED_PIN, 1);
      sleep_ms(500);
      printf("Hello %u\n", i++);
    }

}

