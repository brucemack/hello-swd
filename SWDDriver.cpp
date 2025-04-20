#include <stdio.h>
#include "SWDDriver.h"
#include "hardware/gpio.h"
#include "pico/time.h"

namespace kc1fsz {

static const char* SELECTION_ALERT = "0100_1001_1100_1111_1001_0000_0100_0110_1010_1001_1011_0100_1010_0001_0110_0001_"
                                     "1001_0111_1111_0101_1011_1011_1100_0111_0100_0101_0111_0000_0011_1101_1001_1000";

static const char* ACTIVATION_CODE = "0000_0101_1000_1111";

#define DAP_ADDR_CORE0 (0x01002927)

SWDDriver::SWDDriver(unsigned int clock_pin, unsigned int dio_pin) {
    _clkPin = clock_pin;
    _dioPin = dio_pin;
}

void SWDDriver::init() {

    gpio_set_dir(_clkPin, GPIO_OUT);        
    gpio_put(_clkPin, 0);

    gpio_set_dir(_dioPin, GPIO_OUT);        
    gpio_put(_dioPin, 0);
    gpio_set_pulls(_dioPin, false, false);        
}

int SWDDriver::connect() {   

    setDIO(false);
    writeBitPattern("11111111");

    // For ease of tracing
    _delayPeriod();

    // From "Low Pin-count Debug Interfaces for Multi-device Systems"
    // by Michael Williams
    //
    // "Hence the designers of multi-drop SWD chose an unlikely
    // data sequence approach. The selection message consists of a
    // 128-bit selection alert, followed by a protocol selection
    // command. This selection alert method has been adopted by
    // IEEE 1149.7, and multi-drop SWD has adopted the IEEE
    // 1149.7 protocol selection command, ensuring compatibility
    // between the two protocols."

    // Selection alert (128 bits)
    writeSelectionAlert();   
    // ARM Coresight activation code
    writeActivationCode();

    // Here is where we start to follow the protocol described
    // in ARM IHI0031A section 5.4.1.
    //
    // More specifically, this from IHI0031G section B4.3.4:
    //
    // B4.3.4 Target selection protocol, SWD protocol version 2
    // ---------------------------------------------------------
    // 1. Perform a line reset. See Figure B4-9 on page B4-124.
    // 2. Write to DP register 0xC, TARGETSEL, where the data indicates 
    //    the selected target. The target response must be ignored. See Figure 
    //    B4-9 on page B4-124.
    // 3. Read from the DP register 0x0, DPIDR, to verify that the target 
    // has been successfully selected.

    // Line reset
    writeLineReset();

    // Testing verified that this is required
    writeBitPattern("00000000");

    // For ease of tracing
    _delayPeriod();

    // DP TARGETSEL, DP for Core 0.  We ignore the ACK here!
    // At this point there are multiple cores listening on the 
    // SWD bus (i.e. multi-drop) so this selection determines 
    // which one is activated. This is the reason that there 
    // is no ACK - we don't want the cores interfering with each 
    // other's response.
    //
    // Some RP2040-specific commentary (from the datasheet):
    //
    // Debug access is via independent DAPs (one per core) attached to a 
    // shared multidrop SWD bus (SWD v2). Each DAP will only respond to 
    // debug commands if correctly addressed by a SWD TARGETSEL command; 
    // all others tristate their outputs.  Additionally, a Rescue DP (see 
    // Section 2.3.4.2) is available which is connected to system control 
    // features. Default addresses of each debug port are given below:
    //
    // Core 0   : 0x01002927
    // Core 1   : 0x11002927
    // Rescue DP: 0xf1002927
    // 
    // The Instance IDs (top 4 bits of ID above) can be changed via a sysconfig 
    // register which may be useful in a multichip application. However note 
    // that ID=0xf is reserved for the internal Rescue DP (see Section 2.3.4.2).
    //
    if (const auto r = writeDP(0b1100, DAP_ADDR_CORE0, true); r != 0) 
        return -1;

    // Read from the ID CODE register
    if (const auto r = readDP(0b0000); r.has_value()) {
        // Good outcome
    }
    else {
        printf("IDCODE ERROR %d\n", r.error());
        return -2;
    }

    // Abort (in case anything is in process)
    if (const auto r = writeDP(0b0000, 0x0000001e); r != 0)
        return -3;

    // Set AP and DP bank 0
    if (const auto r = writeDP(0b1000, 0x00000000); r != 0)
        return -4;

    // Power up
    if (const auto r = writeDP(0b0100, 0x50000001); r != 0)
        return -5;

    // TODO: POLLING FOR POWER UP NEEDED?

    // Read DP CTRLSEL and check for CSYSPWRUPACK and CDBGPWRUPACK
    if (const auto r = readDP(0b0100); !r.has_value()) {
        return -6;
    } else {
        // TODO: MAKE SURE THIS STILL WORKS
        if ((*r & 0x80000000) && (*r & 0x20000000))
            return 0;
        else
            return -7;
    }

    // DP SELECT - Set AP bank F, DP bank 0
    // [31:24] AP Select
    // [7:4]   AP Bank Select (the active four-word bank)
    if (const auto r = writeDP(0b1000, 0x000000f0); r != 0)
        return -8;

    // Read AP addr 0xFC. [7:4] bank address set previously, [3:0] set here.
    // 0xFC is the AP identification register.
    // The actual data comes back during the DP RDBUFF read
    if (const auto r = readAP(0x0c); !r.has_value())
        return -9;
    // DP RDBUFF - Read AP result
    if (const auto r = readDP(0x0c); !r.has_value()) {
        return -10;
    } else {
        _apId = *r;
    }

    // Leave the debug system in the proper state (post-connect)

    // DP SELECT - Set AP and DP bank 0
    if (const auto r = writeDP(0x8, 0x00000000); r != 0)
        return -11;

    // Write to the AP Control/Status Word (CSW), auto-increment, 32-bit 
    // transfer size.
    //
    // 1010_0010_0000_0000_0001_0010
    // 
    // [5:4] 01  : Auto Increment set to "Increment Single," which increments by the size of the access.
    // [2:0] 010 : Size of the access to perform, which is 32 bits in this case. 
    //
    if (const auto r = writeAP(0b0000, 0x22000012); r != 0)
        return -12;

    return 0;
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
 * IMPORTANT: This function assumes that the appropriate AP
 * and AP register bank 0 have already been selected via a 
 * previous DP SELECT call.  This function does not do those
 * steps in order to save time.
 * 
 * IMPORTANT: This function assumes that the CSW has been 
 * configured for a 4-byte transfer.
 */
int SWDDriver::writeWordViaAP(uint32_t addr, uint32_t data) {
    // Write to the AP TAR register. This is the memory address that we will 
    // be reading/writing from/to.
    if (const auto r = writeAP(0x4, addr); r != 0)
        return r;
    // Write to the AP DRW register
    if (const auto r = writeAP(0xc, data); r != 0)
        return r;
    return 0;
}

   /** 
     * IMPORTANT: This function assumes that the CSW has been 
     * configured for a 4-byte transfer and for auto-increment.
     * 
     * NOTE: Refer to the ARM IHI 0031A document in section 8.2.2.
     * "Automatic address increment is only guaranteed to operate 
     * on the bottom 10-bits of the address held in the TAR."
     */
   int SWDDriver::writeMultiWordViaAP(uint32_t start_addr, const uint32_t* data, 
        unsigned int word_count) {
        
        uint32_t addr = start_addr, last_tar_addr = 0;
        const uint32_t TEN_BITS = 0b1111111111;

        for (unsigned int i = 0; i < word_count; i++) {

            if (i == 0 || (addr & TEN_BITS) != (last_tar_addr & TEN_BITS)) {
                // Write to the AP TAR register. This is the starting memory 
                // address that we will be reading/writing from/to. Since
                // auto-increment is enabled this only needs to happen when 
                // we cross the 10-bit boundaries
                if (const auto r = writeAP(0x4, addr); r != 0)
                    return r;
                last_tar_addr = addr;
            }

            // Write to the AP DRW register
            if (const auto r = writeAP(0xc, *(data + i)); r != 0)
                return r;

            addr += 4;
        }

        return 0;
    }

    /**
     * IMPORTANT: This function assumes that the appropriate AP
     * and AP register bank 0 have already been selected via a 
     * previous DP SELECT call.  This function does not do those
     * steps in order to save time.
     * 
     * IMPORTANT: This function assumes that the CSW has been 
     * configured for a 4-byte transfer.
     */
    std::expected<uint32_t, int> SWDDriver::readWordViaAP(uint32_t addr) {

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

std::expected<uint16_t, int> SWDDriver::readHalfWordViaAP(uint32_t addr) {

    // Write to the AP TAR register. This is the memory address that we will 
    // be reading/writing from/to.
    // Notice that the read is word-aligned
    if (const auto r = writeAP(0x4, addr & 0xfffffffc); r != 0) {
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
        // For the even half-words (i.e. word boundary) just return the 
        // 16 least-significant bytes
        if ((addr & 0x3) == 0)
            return (*r & 0xffff);
        // For the odd half-words return the 16 most significant bytes.
        else 
           return (*r >> 16) & 0xffff;
    }
}

/**
 * Polls the S_REGRDY bit of the DHCSR register to find out whether
 * a core register read/write has completed successfully.
 */
int SWDDriver::pollREGRDY(unsigned int timeoutUs) {
    while (true) {
        const auto r = readWordViaAP(ARM_DHCSR);
        if (!r.has_value())
            return -1;
        if (*r & ARM_DHCSR_S_REGRDY)
            return 0;
    }
}

void SWDDriver::_setCLK(bool h) {
    gpio_put(_clkPin, h ? 1 : 0);
}

void SWDDriver::_setDIO(bool h) {
    gpio_put(_dioPin, h ? 1 : 0);    
}

bool SWDDriver::_getDIO() {
    return gpio_get(_dioPin) == 1;
}

void SWDDriver::_holdDIO() {
    gpio_set_dir(_dioPin, GPIO_OUT);                
}

void SWDDriver::_releaseDIO() {
    gpio_set_pulls(_dioPin, false, false);        
    gpio_set_dir(_dioPin, GPIO_IN);                
}

std::expected<uint32_t, int> SWDDriver::_read(bool isAP, uint8_t addr) {

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


int SWDDriver::_write(bool isAP, uint8_t addr, uint32_t data, bool ignoreAck) {

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
        //printf("_write() ACK %d\n", ack);
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

void SWDDriver::writeBit(bool b) {
    // Setup the outbound data
    _setDIO(b);
    _delayPeriod();
    // Slave will capture the data on this rising edge
    _setCLK(true);
    _delayPeriod();
    _setCLK(false);
}

bool SWDDriver::readBit() {
    // NOTE: This makes it look like the data is already setup by the previous
    // falling clock edge?
    _delayPeriod();
    bool r = _getDIO();
    _setCLK(true);
    _delayPeriod();
    _setCLK(false);
    return r;
}

void SWDDriver::writeBitPattern(const char* pattern) {
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

void SWDDriver::writeLineReset() {
    for (unsigned int i = 0; i < 64; i++)
        writeBit(true);
}

void SWDDriver::writeSelectionAlert() {
    writeBitPattern(SELECTION_ALERT);
}

void SWDDriver::writeActivationCode() {
    writeBitPattern(ACTIVATION_CODE);
}

void SWDDriver::_delayPeriod() {
    sleep_us(1);
}

}
