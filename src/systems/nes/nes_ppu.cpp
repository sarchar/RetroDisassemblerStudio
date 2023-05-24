#include <memory>

#include "util.h"

#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_ppu.h"

using namespace std;

namespace NES {

// Weird PPU latch system means all writes and reads set the latch, but some registers like
// PPUCONT aren't readable and return the latched value instead
class PPUView : public MemoryView {
public:
    PPUView(shared_ptr<PPU> const& _ppu)
        : ppu(_ppu) {}
    virtual ~PPUView() {}

    u8 Read(u16 address) override {
        u16 reg = address & 0x07;
        u8 ret = latch_value;

        switch(reg) {
        case 0x00: // PPUCONT not readable
        case 0x01: // PPUMASK not readable
            break;

        case 0x02: // PPUSTAT
             ret = ppu->ppustat;
             ppu->vblank = 0;

             // reset the address latch
             ppu->vram_address_latch = 8;
             break;

        default:
            cout << "[PPUView::Read] read from $" << hex << address << endl;
            ret = 0;
            break;
        }

        latch_value = ret;
        return ret;
    }

    void Write(u16 address, u8 value) override {
        latch_value = value;

        u16 reg = address & 0x07;
        switch(reg) {
        case 0x00: // PPUCONT
            // if the PPU is currentinly in vblank and the PPUSTAT flag is still set to 1, changing the NMI enable flag triggers NMI immediately
            if(ppu->vblank && !ppu->enable_nmi && (value & 0x80)) ppu->nmi();

            ppu->ppucont = value;
            break;

        case 0x01: // PPUMASK
            ppu->ppumask = value;
            break;

        case 0x02: // PPUSTAT not writable
            break;

        case 0x06: // PPUADDR write x2
            ppu->vram_address = (u16)((ppu->vram_address & ((u16)0x00FF << (ppu->vram_address_latch ^ 0x08))) | ((u16)value << ppu->vram_address_latch));
            ppu->vram_address_latch ^= 0x08;
            //if(ppu->vram_address_latch == 8) cout << hex << ppu->vram_address << endl;
            break;

        case 0x07: // PPUDATA
            WritePPU(ppu->vram_address, value);
            ppu->vram_address += (ppu->vram_increment << 5);
            break;

        default:
            cout << "[PPUView::Write] write $" << hex << (int)value << " to $" << hex << address << endl;
            break;
        }
    }

    // map these to the PPU bus
    u8 ReadPPU(u16 address) override {
        return ppu->Read(address);
    };

    void WritePPU(u16 address, u8 value) override {
        ppu->Write(address, value);
    };

private:
    shared_ptr<PPU> ppu;
    u8              latch_value;
};

PPU::PPU(nmi_function_t const& nmi_function, read_func_t const& read_func, write_func_t const& write_func)
    : nmi(nmi_function), Read(read_func), Write(write_func)
{
}

PPU::~PPU()
{
}

void PPU::Reset()
{
    enable_nmi = 0;
    scanline = 0;
    cycle = 0;
}

shared_ptr<MemoryView> PPU::CreateMemoryView()
{
    return make_shared<PPUView>(shared_from_this());
}

void PPU::Step()
{
    // perform current scanline/cycle

    if(scanline < 240 || scanline == 261) {
        if(cycle == 0) {
        } else if(cycle < 257) {
            vblank = 0; // doesn't hurt to set this every non-vblank cycle, but the first time it'll matter is (scanline=261,cycle=1)
        } else if(cycle < 321) {
        } else if(cycle < 337) {
        } else /* cycle < 341 */ {
        }
    } else if(scanline == 241) {
        if(cycle == 1) {
            vblank = 1;
            if(enable_nmi) nmi();
        }
    }

    // end of step, increment cycle
    if(++cycle == 341) {
        if(++scanline == 262) {
            scanline = 0;
        }
        cycle = 0;
    }
}

}
