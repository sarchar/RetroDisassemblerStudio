#include <memory>

#include "util.h"

#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_ppu.h"

using namespace std;

namespace NES {

#define RGB(r,g,b) (((u32)(b) << 16) | ((u32)(g) << 8) | (u32)(r))
static int const rgb_palette_map[] = {
    RGB(82, 82, 82),
    RGB(1, 26, 81),
    RGB(15, 15, 101),
    RGB(35, 6, 99),
    RGB(54, 3, 75),
    RGB(64, 4, 38),
    RGB(63, 9, 4),
    RGB(50, 19, 0),
    RGB(31, 32, 0),
    RGB(11, 42, 0),
    RGB(0, 47, 0),
    RGB(0, 46, 10),
    RGB(0, 38, 45),
    RGB(0, 0, 0),
    RGB(0, 0, 0),
    RGB(0, 0, 0),
    RGB(160, 160, 160),
    RGB(30, 74, 157),
    RGB(56, 55, 188),
    RGB(88, 40, 184),
    RGB(117, 33, 148),
    RGB(132, 35, 92),
    RGB(130, 46, 36),
    RGB(111, 63, 0),
    RGB(81, 82, 0),
    RGB(49, 99, 0),
    RGB(26, 107, 5),
    RGB(14, 105, 46),
    RGB(16, 92, 104),
    RGB(0, 0, 0),
    RGB(0, 0, 0),
    RGB(0, 0, 0),
    RGB(254, 255, 255),
    RGB(105, 158, 252),
    RGB(137, 135, 255),
    RGB(174, 118, 255),
    RGB(206, 109, 241),
    RGB(224, 112, 178),
    RGB(222, 124, 112),
    RGB(200, 145, 62),
    RGB(166, 167, 37),
    RGB(129, 186, 40),
    RGB(99, 196, 70),
    RGB(84, 193, 125),
    RGB(86, 179, 192),
    RGB(60, 60, 60),
    RGB(0, 0, 0),
    RGB(0, 0, 0),
    RGB(254, 255, 255),
    RGB(190, 214, 253),
    RGB(204, 204, 255),
    RGB(221, 196, 255),
    RGB(234, 192, 249),
    RGB(242, 193, 223),
    RGB(241, 199, 194),
    RGB(232, 208, 170),
    RGB(217, 218, 157),
    RGB(201, 226, 158),
    RGB(188, 230, 174),
    RGB(180, 229, 199),
    RGB(181, 223, 228),
    RGB(169, 169, 169),
    RGB(0, 0, 0),
    RGB(0, 0, 0)
};

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

        case 0x07: // PPUDATA
            ret = ppu->vram_read_buffer;
            ppu->vram_read_buffer = ReadPPU(ppu->vram_address);
            ppu->vram_address += (ppu->vram_increment ? 32 : 1);
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
        address &= 0x3FFF;
        
        // internal to the PPU is palette ram
        if((address & 0x3F00) == 0x3F00) {
            return ppu->palette_ram[address & 0x1F];
        } else {
            return ppu->Read(address);
        }
    };

    void WritePPU(u16 address, u8 value) override {
        address &= 0x3FFF;

        if((address & 0x3F00) == 0x3F00) {
            int palette_index = address & 0x1F;
            if((palette_index & 0x03) == 0) {
                // Mirror 0x10, 0x14, 0x18, 0x1C -> 0x00, 0x04, 0x08, 0x0C
                ppu->palette_ram[palette_index | 0x10] = value;
                palette_index &= ~0x10;
            }
            ppu->palette_ram[palette_index] = value;
        } else {
            ppu->Write(address, value);
        }
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
            } else /* cycle < 341 */ { // cycles 337..340
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

    // pipeline the color generation for 4 cycles
    int ret_color = color_pipeline[0];
    color_pipeline[0] = color_pipeline[1];
    color_pipeline[1] = color_pipeline[2];
    color_pipeline[2] = color;

    return ret_color;
}

int PPU::InternalStep()
{
    // if both sprites and bg are disabled, rendering is disabled, and we don't do any memory accesses
    if(!(show_sprites || show_background)) return 0;

    // phase 1 needs the shift register fully shifted 8 times
    // shift registers start shifting at cycle 2, and the first latch of the shift register happens at cycle 9
    // so we can be sure (at cycles 2, 3, 4, 5, 6, 7, 8, and 9) 8 bits are shifted out before the latch at cycle 9
    // TODO Shift() is also where things like sprite 0 hit are setup. Don't shift in cycles 337..340
    if(cycle >= 2 && cycle <= 337) Shift();

    // setup address and latch data depending on the read phase
    int phase = cycle % 8;
    switch(phase) {
    case 1:
    {
        if(cycle != 1) {
            // fill shift registers. the first such event happens on cycle 9, and then 17, 25, ..
            // for the first two tiles, this latch occurs at cycles 329 and 337
            attribute_byte = attribute_next_byte;
            attribute_next_byte = attribute_latch;
            background_lsbits = (u16)background_lsbits_latch | (background_lsbits & 0xFF00);
            background_msbits = (u16)background_msbits_latch | (background_msbits & 0xFF00);
        }

        // initialize base address to 0x2000, 0x2400, 0x2800, 0x2C00
        vram_address = 0x2000 | (base_nametable_address << 10);

        // x_pos/y_pos two tiles ahead (it's set to zero 20 cycles before the new scanline)
        int x_tile = ((x_pos + (int)scroll_x) >> 3);
        if(x_tile >= 32) vram_address ^= 0x400; // change nametables horizontally 

        int y_tile = (y_pos >> 3);
        if(y_tile >= 30) vram_address ^= 0x800; // change vertically vertically

        vram_address |= (x_tile & 0x1F) + ((y_tile & 0x1F) << 5);
        //cout << "x=" << dec << x_pos << " y=" << y_pos << " NT addr: $" << hex << vram_address << endl;
        break;
    }

    case 2:
        // latch NT byte
        nametable_latch = Read(vram_address);
        break;

    case 3:
    {
        // setup attribute address
        // take out the nametable base
        int offset = vram_address & 0x3FF;

        // 32 tiles per row, 4 x-tiles represented per attribute byte
        // every 32 * 4 y-tiles = 0x80 tiles, increment attribute table address by 8 bytes 
        // (8 attribute bytes per 0x80 tiles)
        int attribute_addr = (offset & 0x380) >> 4;  // equiv: (offset >> 7) << 3;

        // and add in one for every 4 tiles in the x direction. 0x20 tiles per row, divided by 4
        attribute_addr += (offset & 0x1F) >> 2;

        // and then add the base of the attribute table
        vram_address = (vram_address & 0x2C00) | 0x3C0 | attribute_addr;
        break;
    }
    case 4:
        // latch attribute byte
        attribute_latch = Read(vram_address);
        break;
    case 5:
        // setup lsbits tile address
        vram_address = (background_pattern_table_address << 12) + (nametable_latch << 4) + (y_pos & 0x07);
        break;
    case 6:
        // latch lsbits tile byte
        background_lsbits_latch = Read(vram_address);
        break;
    case 7:
        // setup msbits tile address
        vram_address += 8;
        break;
    case 0:
        // latch msbits tile byte
        background_msbits_latch = Read(vram_address);
        break;
    }

    return DeterminePixel();
}

void PPU::Shift()
{
    background_lsbits <<= 1;
    background_msbits <<= 1;
}

// DeterminePixel has no side effects
// Rendering a pixel seems so easy when all the hard work of determining addresses and shifts
// is done beforehand
int PPU::DeterminePixel() const
{
    // determine tile color (2 bit)
    int fine_x = scroll_x & 7;
    u8 bit0 = (u8)((background_lsbits >> (15 - fine_x)) & 0x01);
    u8 bit1 = (u8)((background_msbits >> (15 - fine_x)) & 0x01);
    int tile_color = ((bit1 << 1) | bit0);

    // determine attribute bits (palette index), 2 bits
    // left/right nibble switches on y_pos every 16 rows
    int y_shift = (y_pos & 0x10) >> 2; // y_shift = 0 or 4
    int attribute_half = (attribute_byte >> y_shift) & 0x0F;

    // left/right byte of said nibble switches on x_pos every 16 pixels
    // (x_pos is always two tiles = 16 pixels ahead)
    int x_shift = ((x_pos + scroll_x - 16) & 0x10) >> 3; // x_shift is 0 or 2
    int attr = (attribute_half >> x_shift) & 0x03;

    // NES palette lookup is 4 bits/16 colors
    u8  nes_palette_index = ((attr << 2) | tile_color);

    // translate palette + tile_color into RGB
    int nes_color = palette_ram[tile_color == 0 ? 0 : nes_palette_index];

    int color = rgb_palette_map[nes_color & 0x3F];

    return color;
}

}
