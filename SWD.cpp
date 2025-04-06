#include <stdio.h>
#include "SWD.h"
#include "hardware/gpio.h"
#include "pico/time.h"

namespace kc1fsz {

static const char* SELECTION_ALERT = "0100_1001_1100_1111_1001_0000_0100_0110_1010_1001_1011_0100_1010_0001_0110_0001_"
                                     "1001_0111_1111_0101_1011_1011_1100_0111_0100_0101_0111_0000_0011_1101_1001_1000";

static const char* ACTIVATION_CODE = "0000_0101_1000_1111";

// This is supposed to be AEEE_EEE6 (MSB first), but only the last 30 bits are used
static const char* JTAG_TO_DS_CONVERSION = "1010_1110_1110_1110_1110_1110_1110_0110";


#define CLK_PIN (2)
#define DIO_PIN (3)

void SWD::init() {
    gpio_set_dir(CLK_PIN, GPIO_OUT);        
    gpio_put(CLK_PIN, 0);

    gpio_set_dir(DIO_PIN, GPIO_OUT);        
    gpio_put(DIO_PIN, 0);
    gpio_set_pulls(DIO_PIN, false, false);        
}

void SWD::_setCLK(bool h) {
    gpio_put(CLK_PIN, h ? 1 : 0);
}

void SWD::_setDIO(bool h) {
    gpio_put(DIO_PIN, h ? 1 : 0);    
}

bool SWD::_getDIO() {
    return gpio_get(DIO_PIN) == 1;
}

void SWD::_holdDIO() {
    gpio_set_dir(DIO_PIN, GPIO_OUT);                
}

void SWD::_releaseDIO() {
    gpio_set_pulls(DIO_PIN, false, false);        
    gpio_set_dir(DIO_PIN, GPIO_IN);                
}

std::expected<uint32_t, int> SWD::_read(bool isAP, uint8_t addr) {

    // The only variable bits are the address and the DP/AP flag
    unsigned int ones = 0;
    if (addr & 0b0100)
        ones++;
    if (addr & 0b1000)
        ones++;
    if (isAP)
        ones++;
    // Read flag
    ones++;

    // Start
    writeBit(true);
    // 0=DP, 1=AP
    writeBit(isAP);
    // 1=Read
    writeBit(true);
    // Address[3:2] (LSB first)
    writeBit((addr & 0b0100) != 0);
    writeBit((addr & 0b1000) != 0);
    // This system uses even parity, so an extra one should be 
    // added only if the rest of the count is odd.
    writeBit((ones % 2) == 1);
    // Stop
    writeBit(false);

    // Park 
    writeBit(true);
    _releaseDIO();

    _delayPeriod();

    // One cycle turnaround 
    readBit();

    // IMPORTANT: For ease of display only!
    _delayPeriod();

    // Read three bits (LSB first)
    uint8_t ack = 0;
    if (readBit()) ack |= 1;
    if (readBit()) ack |= 2;
    if (readBit()) ack |= 4;


    // 0b001 is OK
    if (ack != 0b001) {
        // TODO: DECIDE HOW TO DEAL WITH THIS
        _holdDIO();
        return std::unexpected(-1);
    }

    // IMPORTANT: For ease of display only!
    _delayPeriod();

    // Read data, LSB first
    uint32_t data = 0;
    ones = 0;
    for (unsigned int i = 0; i < 32; i++) {
        bool bit = readBit();
        ones += (bit) ? 1 : 0;
        data = data >> 1;
        data |= (bit) ? 0x80000000 : 0;
    }

    // IMPORTANT: For ease of display only!
    _delayPeriod();

    // Read parity
    readBit();

    _delayPeriod();

    // One cycle turnaround 
    _holdDIO();
    writeBit(false);

    return data;
}


int SWD::_write(bool isAP, uint8_t addr, uint32_t data, bool ignoreAck) {

    // The only variable bits are the address and the DP/AP flag
    unsigned int ones = 0;
    if (addr & 0b0100)
        ones++;
    if (addr & 0b1000)
        ones++;
    if (isAP)
        ones++;

    // Start
    writeBit(true);
    // 0=DP, 1=AP
    writeBit(isAP);
    // 0=Write
    writeBit(false);
    // Address[3:2] (LSB first)
    writeBit((addr & 0b0100) != 0);
    writeBit((addr & 0b1000) != 0);
    // This system uses even parity, so an extra one should be 
    // added only if the rest of the count is odd.
    writeBit((ones % 2) == 1);
    // Stop
    writeBit(false);
    
    // Park: drive the DIO high and leave it there
    writeBit(true);
    _releaseDIO();

    // IMPORTANT: For ease of display/debug only
    _delayPeriod();
    
    // One cycle turnaround 
    readBit();

    // IMPORTANT: For ease of display/debug only
    _delayPeriod();

    // Read three bits (LSB first)
    uint8_t ack = 0;
    if (readBit()) ack |= 1;
    if (readBit()) ack |= 2;
    if (readBit()) ack |= 4;

    // One cycle turnaround 
    _holdDIO();
    writeBit(false);

    // 001 is OK
    if (!ignoreAck) {
        printf("_write() ACK %d\n", ack);
        if (ack != 0b001) {
            return -1;
        }
    }

    // Write data, LSB first
    ones = 0;
    for (unsigned int i = 0; i < 32; i++) {
        bool bit = (data & 1) == 1;
        ones += (bit) ? 1 : 0;
        writeBit(bit);
        data = data >> 1;
    }

    // IMPORTANT: For ease of display/debug only
    _delayPeriod();

    // Write parity in order to make the one count even
    writeBit((ones % 2) == 1);

    return 0;
}

void SWD::writeBit(bool b) {
    // Setup the outbound data
    _setDIO(b);
    _delayPeriod();
    // Slave will capture the data on this rising edge
    _setCLK(true);
    _delayPeriod();
    _setCLK(false);
}

bool SWD::readBit() {
    // NOTE: This makes it look like the data is already setup by the previous
    // falling clock edge?
    _delayPeriod();
    bool r = _getDIO();
    _setCLK(true);
    _delayPeriod();
    _setCLK(false);
    return r;
}

void SWD::writeBitPattern(const char* pattern) {
    const char* b = pattern;
    while (*b != 0) {
        if (*b == '0') {
            writeBit(false);
        } else if (*b == '1') {
            writeBit(true);
        }
        b++;
    }
}

void SWD::writeLineReset() {
    for (unsigned int i = 0; i < 64; i++)
        writeBit(true);
}

void SWD::writeSelectionAlert() {
    writeBitPattern(SELECTION_ALERT);
}

void SWD::writeActivaionCode() {
    writeBitPattern(ACTIVATION_CODE);
}

void SWD::writeJTAGToDSConversion() {
    writeBitPattern(JTAG_TO_DS_CONVERSION);
}

void SWD::_delayPeriod() {
    sleep_us(1);
    //busy_wait_us_32(1);9999999
}

}
