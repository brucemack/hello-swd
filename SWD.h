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
     * @param addr 4-bit address, but [1:0] are always zero
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

    /**
     * Writes a 32-bit word into the processor memory space via the MEM-AP
     * mechanism.  This involves seting the AP TAR register first and then 
     * writing to the AP DRW register.
     * 
     * @param addr The processor address
     * @param data The data to write
     * @returns 0 on success.
     * 
     * IMPORTANT: This funciton assumes that the appropriate AP
     * and AP register bank 0 have already been selected via a 
     * previous DP SELECT call.  This function does not do those
     * steps in order to save time.
     */
    int writeWordViaAP(uint32_t addr, uint32_t data) {
        // Write to the AP TAR register. This is the memory address that we will 
        // be reading/writing from/to.
        if (const auto r = writeAP(0x4, addr); r != 0) {
            return r;
        }
        // Write to the AP DRW register
        if (const auto r = writeAP(0xc, data); r != 0) {
            return r;
        } else {
            return 0;
        }
    }

    /**
     * IMPORTANT: This funciton assumes that the appropriate AP
     * and AP register bank 0 have already been selected via a 
     * previous DP SELECT call.  This function does not do those
     * steps in order to save time.
     */
    std::expected<uint32_t, int> readWordViaAP(uint32_t addr) {

        // Write to the AP TAR register. This is the memory address that we will 
        // be reading/writing from/to.
        if (const auto r = writeAP(0x4, addr); r != 0) {
            return std::unexpected(r);
        }
        // Read from the AP DRW register (actual data is buffered and comes later)
        if (const auto r = readAP(0xc); !r.has_value()) {
            return std::unexpected(r.error());
        }
        // Fetch result of previous AP read
        if (const auto r = readDP(0xc); !r.has_value()) {
            return std::unexpected(r.error());
        } else {
            return *r;
        }
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
