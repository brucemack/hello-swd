#include "SWD.h"

namespace kc1fsz {

std::expected<void, std::sring> SWD::_write(bool isAP, uint8_t addr, uint32_t data) {

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
        return std::unexpected(-1);

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
}

void SWD::writeAP(uint8_t addr, uint32_t data) {

}

uint32_t SWD::readDP(uint8_t addr) {

}

uint32_t SWD::readAP(uint8_t addr) {

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

void SWD::lineReset() {
    for (unsigned int i = 0; i < 64; i++)
        writeBit(true);
}

}
