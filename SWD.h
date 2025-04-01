#ifndef _SWD_h
#define _SWD_h

#include <expected>
#include <string>

typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

namespace kc1fsz {

class SWD {
public:

    // NOTE: Slave captures outbound data on the rising edge of the clock
    void writeBit(bool b);
    // NOTE: Master captures inbound data on the falling edge of the clock
    bool readBit();

    void lineReset();
    void init();

    std::expected<void, int> writeDP(uint8_t addr, uint32_t data) {
        return _write(false, addr, data);
    }

    std::expected<void, int> writeAP(uint8_t addr, uint32_t data) {
        return _write(true, addr, data);
    }

    std::expected<uint32_t, int> readDP(uint8_t addr) {
        return _read(false, addr);
    }

    std::expected<uint32_t, int> readAP(uint8_t addr) {
        return _read(true, addr);
    }

protected:

    std::expected<void, int> _write(bool isAP, uint8_t addr, uint32_t data);
    std::expected<uint32_t, int> _read(bool isAP, uint8_t addr);

    void _setCLK(bool h);
    void _setDIO(bool h);
    void _setDIOInput(bool b);
    bool _getDIO();
    void _holdDIO();
    void _releaseDIO();
    void _delaySetup();
    void _delayHold();
    void _delayPeriod();
    unsigned int _countOnes(uint8_t b);
};

}

#endif
