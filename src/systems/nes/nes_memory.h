#pragma once

#include "util.h"

namespace NES {

// SystemMemoryLocation dials into a specific byte within the system. It has enough information to select which
// segment of the system (RAM, SRAM, etc) as well as which ROM bank, overlay or any psuedo location that may exist.
struct GlobalMemoryLocation {
    GlobalMemoryLocation() { 
        address      = 0;
        is_chr       = false;
        prg_rom_bank = 0;
        chr_rom_bank = 0;
    }

    // 0x00-0xFF: zero page
    // 0x100-0x1FF: stack
    // 0x200-0x7FF: RAM
    // 0x6000-7FFF: SRAM
    // 0x8000-FFFF: ROM
    u16 address;

    // set to true if we're reading CHR-RAM
    bool is_chr;

    // used only for ROM
    u16 prg_rom_bank;

    // used only for CHR
    u16 chr_rom_bank;

    void Increment() {
    }

    template<typename T>
    GlobalMemoryLocation operator+(T const& v) const {
        GlobalMemoryLocation ret(*this);
        ret.address += v; // TODO wrap, increment banks, etc
        return ret;
    }

    friend std::ostream& operator<<(std::ostream& stream, const GlobalMemoryLocation& p); 
};

}
