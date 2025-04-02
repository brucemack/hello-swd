#include "SWD.h"

namespace kc1fsz {

static const char* SELECTION_ALERT = "0100_1001_1100_1111_1001_0000_0100_0110_1010_1001_1011_0100_1010_0001_0110_0001_"
                                     "1001_0111_1111_0101_1011_1011_1100_0111_0100_0101_0111_0000_0011_1101_1001_1000";

static const char* ACTIVATION_CODE = "0000_0101_1000_1111";

std::expected<uint32_t, int> SWD::_read(bool isAP, uint8_t addr) {

    // The only variable bits are the address and the DP/AP flag
    unsigned int ones = _countOnes(addr & 0b11);
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
    // Address[2:3] (LSB first)
    writeBit((addr & 1) == 1);
    writeBit((addr & 2) == 2);
    // This system uses even parity, so an extra one should be 
    // added only if the rest of the count is odd.
    writeBit((ones % 2) == 1);
    // Stop
    writeBit(false);
    // Park 
    writeBit(true);
    
    // One cycle turnaround 
    _releaseDIO();
    readBit();

    // Read three bits (LSB first)
    uint8_t ack = 0;
    if (readBit()) ack |= 1;
    if (readBit()) ack |= 2;
    if (readBit()) ack |= 4;

    if (ack != 1) {
        // TODO: DECIDE HOW TO DEAL WITH THIS
        _holdDIO();
        return std::unexpected(-1);
    }

    // Read data, LSB first
    uint32_t data = 0;
    ones = 0;
    for (unsigned int i = 0; i < 32; i++) {
        bool bit = readBit();
        ones += (bit) ? 1 : 0;
        writeBit(bit);
        data = data >> 1;
        data = data | (bit) ? 0x80000000 : 0;
    }

    // One cycle turnaround 
    _holdDIO();
    writeBit(false);

    return data;

}


int SWD::_write(bool isAP, uint8_t addr, uint32_t data) {

    // The only variable bits are the address and the DP/AP flag
    unsigned int ones = _countOnes(addr & 0b11);
    if (isAP)
        ones++;

    // Start
    writeBit(true);
    // 0=DP, 1=AP
    writeBit(isAP);
    // 0=Write
    writeBit(false);
    // Address[2:3] (LSB first)
    writeBit((addr & 1) == 1);
    writeBit((addr & 2) == 2);
    // This system uses even parity, so an extra one should be 
    // added only if the rest of the count is odd.
    writeBit((ones % 2) == 1);
    // Stop
    writeBit(false);
    // Park 
    writeBit(true);
    
    // One cycle turnaround 
    _releaseDIO();
    readBit();

    // Read three bits (LSB first)
    uint8_t ack = 0;
    if (readBit()) ack |= 1;
    if (readBit()) ack |= 2;
    if (readBit()) ack |= 4;

    // One cycle turnaround 
    _holdDIO();
    writeBit(false);

    if (ack != 1)
        return -1;

    // Write data, LSB first
    ones = 0;
    for (unsigned int i = 0; i < 32; i++) {
        bool bit = (data & 1) == 1;
        ones += (bit) ? 1 : 0;
        writeBit(bit);
        data = data >> 1;
    }

    // Write parity in order to make the one count even
    writeBit((ones % 2) == 1);

    return 0;
}

void SWD::writeBit(bool b) {
    _setCLK(true);
    _setDIO(b);
    _delaySetup();
    // Slave will capture the data on this falling edge
    _setCLK(false);
    _delayHold();
    _delayPeriod();
}

bool SWD::readBit() {
    // The slave will present the data on this rising edge
    _setCLK(true);
    _delaySetup();
    bool r = _getDIO();
    _setCLK(false);
    _delayPeriod();
    return r;
}

void SWD::writeBitPattern(const char* pattern) {
    const char* b = pattern;
    while (b != 0) {
        if (*b == '0') {
            writeBit(false);
        } else if (*b == '1') {
            writeBit(true);
        }
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

}
