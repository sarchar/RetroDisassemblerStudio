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

        case 0x05: // PPUSCRL write x2
            // PPUSCRL uses the address latch -- it affects PPUADDR
            if(ppu->vram_address_latch) ppu->scroll_x = value;
            else                        ppu->scroll_y = value;
            ppu->vram_address_latch ^= 0x08;
            break;

        case 0x06: // PPUADDR write x2
            ppu->vram_address = (u16)((ppu->vram_address & ((u16)0x00FF << (ppu->vram_address_latch ^ 0x08))) | ((u16)value << ppu->vram_address_latch));
            ppu->vram_address_latch ^= 0x08;
            //if(ppu->vram_address_latch == 8) cout << hex << ppu->vram_address << endl;
            break;

        case 0x07: // PPUDATA
            WritePPU(ppu->vram_address, value);
            ppu->vram_address += (ppu->vram_increment ? 32 : 1);
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

    scroll_x = 0;
    scroll_y = 0;

    x_pos = 16;
    y_pos = 0;
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
                x_pos += 1;
            } else if(cycle < 321) {   // cycles 257..320
                // two unused vram fetches (phases 1..4)
                if(cycle == 257) { // restart the next x position
                    if(scanline == 261) y_pos = scroll_y;
                    else                y_pos += 1;
                    x_pos = 0;
                }
            } else if(cycle < 337) {   // cycles 321..336
                // first two tiles of the next line
                color = InternalStep();
                x_pos += 1;
            } else /* cycle < 341 */ { // cycles 337..341
                // two unused vram fetches (phases 1..4), which latches the second of the first two tiles
                // but we have to make sure x_pos doesn't increment here
                InternalStep();
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

    // phase 1 needs the shift register fully shifted 8 times
    // shift registers start shifting at cycle 2, and the first latch of the shift register happens at cycle 9
    // so we can be sure (at cycles 2, 3, 4, 5, 6, 7, 8, and 9) 8 bits are shifted out before the latch at cycle 9
    // TODO Shift() is also where things like sprite 0 hit are setup
    if(cycle >= 2) Shift();

    // setup address and latch data depending on the read phase
    int phase = cycle % 8;
    switch(phase) {
    case 1:
    {
        if(cycle != 1) {
            // fill shift registers. the first such event happens on cycle 9, and then 17, 25, ..
            nametable_byte = nametable_latch;
            attribute_byte = attribute_latch;
            background_lsbits = ((u16)background_lsbits_latch << 8) | (background_lsbits & 0x00FF);
            // msbits are on the data bus
            background_msbits = ((u16)Read(vram_address) << 8) | (background_msbits & 0x00F);
        }

        // initialize base address to 0x2000, 0x2400, 0x2800, 0x2C00
        vram_address = 0x2000 | (base_nametable_address << 10);

        // x_pos/y_pos two tiles ahead (it's set to zero 20 cycles before the new scanline)
        int x_tile = ((x_pos + (int)scroll_x) >> 3);
        if(x_tile >= 32) vram_address ^= 0x400; // change nametables horizontally 

        int y_tile = (y_pos >> 3);
        if(y_tile >= 30) vram_address ^= 0x800; // change vertically vertically

        vram_address |=  (x_tile & 0x1F) + ((y_tile & 0x1F) << 5);
        //cout << "x=" << dec << x_pos << " y=" << y_pos << " NT addr: $" << hex << vram_address << endl;
        break;
    }

    case 2:
        // latch NT byte
        nametable_latch = Read(vram_address);
        //cout << "nametable byte = $" << hex << (int)nametable_latch << endl;
        break;

    case 3:
    {
        // setup attribute address
        // take out the nametable base
        int offset = vram_address & 0x3FF;

        // every 32 * 4 = 0x80 tiles, increment attribute table address by 8 bytes (8 attr per row)
        int attribute_addr = (offset & 0x380) >> 4;  // equiv: (base >> 7) << 3;

        // and add in one for every 4 tiles in the x direction. 0x20 tiles per row, divided by 4
        attribute_addr += (offset & 0x1F) >> 2;

        // and then add the base of the attribute table
        vram_address = (vram_address & 0x2C00) + 0x3C0 + attribute_addr;

        break;
    }
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

    return DeterminePixel();
}

void PPU::Shift()
{
}

// DeterminePixel has no side effects
int PPU::DeterminePixel() const
{
    //int color = 0xFF000000 + (0x000001 * (cycle & 255)) + (0x000100 * scanline);
    //int color = (0x0100 * x_pos) + y_pos; //nametable_byte;
    int color = 0x40 * attribute_byte;

    //if(x_pos >= 16 && x_pos <= 23 && y_pos >= 0 && y_pos <= 7) {
    //    cout << "x=" << dec << x_pos << "y=" << y_pos << "vram_address=$" << hex << vram_address << " byte=$" << color << endl;
    //}
    return 0xFF000000 | color;
}

}
