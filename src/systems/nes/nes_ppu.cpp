#include <memory>

#include "util.h"

#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_ppu.h"

using namespace std;

namespace NES {

class PPUView : public MemoryView {
public:
    PPUView(shared_ptr<PPU> const& _ppu)
        : ppu(_ppu) {}
    virtual ~PPUView() {}

    u8 Read(u16 address) override {
        u16 reg = address & 0x07;
        u8 ret;

        switch(reg) {
        case 0x02: // PPUSTAT
             ret = ppu->ppustat;
             ppu->vblank = 0;
             break;

        default:
            cout << "[PPUView::Read] read from $" << hex << address << endl;
            ret = 0;
            break;
        }

        return ret;
    }

    void Write(u16 address, u8 value) override {
        u16 reg = address & 0x07;
        switch(reg) {
        default:
            cout << "[PPUView::Read] Write $" << hex << (int)value << " to $" << hex << address << endl;
            break;
        }
    }

private:
    shared_ptr<PPU> ppu;
};

PPU::PPU(nmi_function_t const& nmi_function)
    : nmi(nmi_function)
{
}

PPU::~PPU()
{
}

void PPU::Reset()
{
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
            nmi();
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
