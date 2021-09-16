#pragma once

#include "wires.h"

// Most 65C816 designs include a transparent latch for the data bank
// and a bi-directional data transceiver.  In addition to the address
// decoding that has to be done to select peripherals, we're building in the 
// latch and transceiver into this module.

class SNESAddressDecoder {
public:
    SNESAddressDecoder();
    
    struct {
        // CPU clock and reset
        Wire phi2      { "SNESAddressDecoder.phi2" };
        Wire reset_n   { "SNESAddressDecoder.reset_n" };

        // connected to the CPU
        Wire     vda   { "SNESAddressDecoder.vda" };
        Wire     vpa   { "SNESAddressDecoder.vpa" };
        Wire     rw_n  { "SNESAddressDecoder.rw_n" };
        Bus<u8>  db    { "SNESAddressDecoder.db" };
        Bus<u16> a_in  { "SNESAddressDecoder.a_in" };

        // connected to the system
        Bus<u8>  d     { "SNESAddressDecoder.d" };
        Bus<u32> a_out { "SNESAddressDecoder.a_out" };

        // various peripheral select lines
        Wire ram_cs_n  { "SNESAddressDecoder.ram_cs_n" };
        Wire rom_cs_n  { "SNESAddressDecoder.rom_cs_n" };
    } pins;

private:
    void SelectPeripheral(u32 address, bool rw_n);
    void DeselectPeripherals();
};
