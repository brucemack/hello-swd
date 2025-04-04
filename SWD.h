#ifndef _SWD_h
#define _SWD_h

#include <cstdint>
#include <expected>

namespace kc1fsz {

class SWD {

public:

    void init();

    // NOTE: Slave captures outbound data on the rising edge of the clock
    void writeBit(bool b);
    // NOTE: Master captures inbound data on the falling edge of the clock
    bool readBit();

    // Pattern is made up of "0", "1", and "_" (ignored)
    void writeBitPattern(const char* pattern);

    /**
     * DIO high, CLK low for 64 cycles
     */
    void writeLineReset();

    void writeSelectionAlert();
    void writeActivaionCode();
    void writeJTAGToDSConversion();

    /**
     * @param ignoreAck When true, the slave-generated acknowledgement is ignored. This
     * is most relevant in the case where a target is being established in a multi-drop
     * system (i.e. the slaves don't assert an ACK in this case).SELECTION_ALERT
     */
    int writeDP(uint8_t addr, uint32_t data, bool ignoreAck = false) {
        return _write(false, addr, data, ignoreAck);
    }

    int writeAP(uint8_t addr, uint32_t data) {
        return _write(true, addr, data);
    }

    std::expected<uint32_t, int> readDP(uint8_t addr) {
        return _read(false, addr);
    }

    std::expected<uint32_t, int> readAP(uint8_t addr) {
        return _read(true, addr);
    }

    void setDIO(bool h) { _setDIO(h); }

protected:

    int _write(bool isAP, uint8_t addr, uint32_t data, bool ignoreAck = false);
    std::expected<uint32_t, int> _read(bool isAP, uint8_t addr);

    void _setCLK(bool h);
    void _setDIO(bool h);
    bool _getDIO();
    void _holdDIO();
    void _releaseDIO();
    void _delayPeriod();
};

}

#endif
