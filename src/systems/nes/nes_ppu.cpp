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
        switch(reg) {
        case 0x02: // PPUSTAT
            return 0x80;

        default:
            cout << "[PPUView::Read] read from $" << hex << address << endl;
            return 0;
        }
    }

    void Write(u16 address, u8 value) override {
        u16 reg = address & 0x07;
        switch(reg) {
        default:
            cout << "[PPUView::Read] Write $" << hex << (int)value << " to $" << hex << address << endl;
        }
    }

private:
    shared_ptr<PPU> ppu;
};

PPU::PPU()
{
}

PPU::~PPU()
{
}

shared_ptr<MemoryView> PPU::CreateMemoryView()
{
    return make_shared<PPUView>(shared_from_this());
}

void PPU::Step()
{
}

}
