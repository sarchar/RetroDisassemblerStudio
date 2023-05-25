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
            //cout << "[PPUView::Write] write $" << hex << (int)value << " to $" << hex << address << endl;
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
    odd = 0;
}

shared_ptr<MemoryView> PPU::CreateMemoryView()
{
    return make_shared<PPUView>(shared_from_this());
}

int PPU::Step(bool& hblank_out, bool& vblank_out)
{
    int color = 0;

    // external wires for this particular pixel
    vblank_out = (scanline >= 240);
    hblank_out = !vblank_out && (cycle < 4 || cycle >= 259); // hblank is delayed because of the color output pipeline

    // perform current scanline/cycle
    if(scanline < 240 || scanline == 261) {
        if(cycle != 0) {
            if(cycle < 257) {   // cycles 1..256
                vblank = 0; // doesn't hurt to set this every cycle, but the first time it'll matter is (scanline=261,cycle=1) when it should first be cleared
                color = InternalStep();
            } else if(cycle < 321) {   // cycles 257..320
                // two unused vram fetches (phases 1..4)
            } else if(cycle < 337) {   // cycles 321..336
                // first two tiles of the next line
                color = InternalStep();
            } else /* cycle < 341 */ { // cycles 337..341
                // two unused vram fetches (phases 1..4)
            }
        }
    } else if(scanline == 241) {
        if(cycle == 1) {
            vblank = 1;
            if(enable_nmi) nmi();
        }
    }

    // end of step, increment cycle
    if(++cycle == 341) {
        cycle = 0;

        if(++scanline == 262) {
            odd ^= 1;
            scanline = 0;

            // odd frames are one clock shorter than normal, they skip the (0,0) cycle
            if(odd) cycle = 1;
        }
    }

    // pipeline the color generation
    int ret_color = color_pipeline[0];
    color_pipeline[0] = color_pipeline[1];
    color_pipeline[1] = color;

    return ret_color;
}

int PPU::InternalStep()
{
    // if both sprites and bg are disabled, rendering is disabled, and we don't do any memory accesses
    if(!(show_sprites || show_background)) return 0;

    // Output pixel before reading from the bus, as phase 1 needs the shift register fully emptied
    int color = (cycle >= 2) ? OutputPixel() : 0;

    // setup address and latch data depending on the read phase
    int phase = cycle % 8;
    switch(phase) {
    case 1:
        // fill shift registers. the first such event happens on cycle 9, and then 17, 25, ..
        nametable_byte = nametable_latch;
        attribute_byte = attribute_latch;
        background_lsbits = ((u16)background_lsbits_latch << 8) | (background_lsbits & 0x00FF);
        // msbits are on the data bus
        background_msbits = ((u16)Read(vram_address) << 8) | (background_msbits & 0x00F);
        
        // setup NT address
        break;
    case 2:
        // latch NT byte
        nametable_latch = Read(vram_address);
        break;
    case 3:
        // setup attribute address
        break;
    case 4:
        // latch attribute byte
        attribute_latch = Read(vram_address);
        break;
    case 5:
        // setup lsbits tile address
        break;
    case 6:
        // latch lsbits tile byte
        background_lsbits_latch = Read(vram_address);
        break;
    case 7:
        // setup msbits tile address
        break;
    case 0:
        // latch msbits tile byte
        break;
    }

    return color;
}

int PPU::OutputPixel()
{
    return 0xFF00FF00;
}

}
