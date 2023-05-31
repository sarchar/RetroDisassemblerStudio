#include <memory>

#include "util.h"

#include "systems/nes/memory.h"
#include "systems/nes/ppu.h"

using namespace std;

namespace Systems::NES {

#define RGB(r,g,b) (((u32)(b) << 16) | ((u32)(g) << 8) | (u32)(r))
int const rgb_palette_map[] = {
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
            ppu->vram_address_latch = 1;
            break;

        case 0x04: // OAMDATA
            ret = ppu->primary_oam[ppu->primary_oam_address];
            // does not increment read address
            break;

        case 0x07: // PPUDATA
            ret = ppu->vram_read_buffer;
            ppu->vram_read_buffer = ReadPPU(ppu->vram_address_v & 0x3FFF);
            // TODO there's a technicality here when reading palettes. 
            // see https://www.nesdev.org/wiki/PPU_registers#Data_.28.242007.29_.3C.3E_read.2Fwrite
            ppu->vram_address_v += (ppu->vram_increment ? 32 : 1);
            // also, reading or writing to this port during rendering will adjust vram_address as well, so
            // that gets updated here too
            ppu->vram_address = ppu->vram_address_v & 0x3FFF; // address actually on the wire
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
            // if the PPU is currentinly in vblank (and the PPUSTAT vblank flag is still set to 1),
            // changing the NMI enable flag from 0 to 1 triggers NMI immediately
            if(ppu->vblank && !ppu->enable_nmi && (value & 0x80)) ppu->nmi();

            ppu->ppucont = value;

            // update vram_address_t
            ppu->vram_address_t = (ppu->vram_address_t & ~0x0C00) | ((int)ppu->base_nametable_address << 10);

            // and scroll_x/y
            ppu->scroll_x = (ppu->scroll_x & ~0x100) | (((int)ppu->base_nametable_address & 0x01) << 8);
            ppu->scroll_y = (ppu->scroll_y & ~0x100) | (((int)ppu->base_nametable_address & 0x02) << 7);
            break;

        case 0x01: // PPUMASK
            ppu->ppumask = value;
            break;

        case 0x02: // PPUSTAT not writable
            break;

        case 0x03: // OAMADDR
            ppu->primary_oam_address = value;
            break;

        case 0x04: // OAMDATA
            if((ppu->show_background || ppu->show_sprites) && ppu->scanline < 240 && ppu->cycle < 257) {
                cout << "[PPUView::Write] warning: write to OAMDATA during rendering" << endl;
            }
            ppu->primary_oam[ppu->primary_oam_address] = value;
            ppu->primary_oam_address += 1;
            break;

        case 0x05: // PPUSCRL write x2
            // PPUSCRL uses the address latch -- it affects PPUADDR
            if(ppu->vram_address_latch) {
                ppu->vram_address_t = (ppu->vram_address_t & ~0x001F) | (value >> 3);
                ppu->fine_x = value & 0x07;
                ppu->scroll_x = (ppu->scroll_x & ~0xFF) | value;
            } else {
                ppu->vram_address_t = (ppu->vram_address_t & ~0x73E0) | ((u16)(value & 0xF8) << 2) | ((u16)(value & 0x07) << 12);
                ppu->scroll_y = (ppu->scroll_x & ~0xFF) | value;
            }
            ppu->vram_address_latch ^= 1;
            break;

        case 0x06: // PPUADDR write x2
            if(ppu->vram_address_latch) {
                ppu->vram_address_t = (ppu->vram_address_t & 0x00FF) | ((value & 0x3F) << 8);
            } else {
                ppu->vram_address_t = (ppu->vram_address_t & 0xFF00) | (u16)value;
                ppu->vram_address_v = ppu->vram_address_t;
            }
            ppu->vram_address_latch ^= 1;
            break;

        case 0x07: // PPUDATA
            WritePPU(ppu->vram_address_v & 0x3FFF, value);
            ppu->vram_address_v += (ppu->vram_increment ? 32 : 1);
            // technically reading or writing to this port during rendering will adjust vram_address as well, so
            // that gets updated here too
            ppu->vram_address = ppu->vram_address_v & 0x3FFF; // address actually on the wire
            break;

        default:
            cout << "[PPUView::Write] write $" << hex << (int)value << " to $" << hex << address << endl;
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
    frame = 0;
    scanline = 0;
    cycle = 0;
    odd = 0;

    scroll_x = 0;
    scroll_y = 0;

    primary_oam_rw = 0;
    secondary_oam_rw = 0;
}

shared_ptr<MemoryView> PPU::CreateMemoryView()
{
    return make_shared<PPUView>(shared_from_this());
}

int PPU::Step(bool& hblank_out, bool& vblank_out)
{
    // used throughout
    rendering_enabled = show_background || show_sprites;

    int color = 0;

    // external wires for this particular pixel
    vblank_out = (scanline >= 240);
    hblank_out = !vblank_out && (cycle < 4 || cycle >= 259); // hblank is delayed because of the color output pipeline

    // perform current scanline/cycle
    if(scanline < 240 || scanline == 261) {
        if(cycle != 0) {
            // sprite 0 hit is cleared on the first pixel of the prerender line
            if(scanline == 261 && cycle == 1) {
                sprite0_hit = 0;
                sprite_zero_hit_buffer = 0;
            }

            if(cycle < 257) {   // cycles 1..256
                vblank = 0; // doesn't hurt to set this every cycle, but the first time it'll matter is (scanline=261,cycle=1) when it should first be cleared
                color = InternalStep(false);
            } else if(cycle < 321) {   // cycles 257..320 (hblank up until bg tile prefetch)
                if(cycle == 257) { // start of hblank
                    // increment the fine Y position in vram_address_v by 1 and move onto the next tile if fine_y overflows
                    if(rendering_enabled) {
                        if((vram_address_v & 0x7000) == 0x7000) {
                            vram_address_v &= ~0x7000;

                            int coarse_y = (vram_address_v & 0x03E0) >> 5;
                            if(coarse_y == 0x1D) { // at 29 roll over to 0 on the next nametable
                                vram_address_v &= ~0x03E0;
                                vram_address_v ^= 0x800;
                            } else if(coarse_y == 0x1F) { // at 31 wrap on the same nametable
                                vram_address_v &= ~0x03E0;
                            } else {
                                vram_address_v += 0x20; 
                            }
                        } else {
                            vram_address_v += 0x1000;
                        }
                    }
                } else if(cycle == 258) {
                    // copy all bits related to horizontal position from vram_address_t to vram_address_v
                    if(rendering_enabled) {
                        vram_address_v = (vram_address_v & ~0x41F) | (vram_address_t & 0x41F);
                    }
                } else if(cycle < 280) {
                    // idle
                } else if(cycle < 305) {
                    // copy all bits related to vertical position from vram_address_t to vram_address_v
                    if(scanline == 261 && rendering_enabled) {
                        vram_address_v = (vram_address_v & ~0x7BE0) | (vram_address_t & 0x7BE0);
                    }
                }

                // and evaluate sprites during hblank (will use vram_address!)
                InternalStep(true);
            } else if(cycle < 337) {   // cycles 321..336
                // first two tiles of the next line
                InternalStep(false);
            } else /* cycle < 341 */ { // cycles 337..340
                // two unused vram fetches (phases 1..4), which latches the second of the first two tiles
                // but we have to make sure x_pos doesn't increment here
                InternalStep(false);
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
            frame += 1;
            scanline = 0;
            odd ^= 1;

            // odd frames are one clock shorter than normal, they skip the (0,0) cycle but only if rendering is enabled
            if(odd && (show_background || show_sprites)) cycle = 1;
        }

        // indicate that sprite 0 is present in secondary OAM (as the 0th element, naturally) on this upcoming scanline
        sprite_zero_present = sprite_zero_next_present;
        sprite_zero_next_present = 0;
    }

    // pipeline the color generation for 4 cycles
    int ret_color = color_pipeline[0];
    color_pipeline[0] = color_pipeline[1];
    color_pipeline[1] = color_pipeline[2];
    color_pipeline[2] = color;

    return ret_color;
}

int PPU::InternalStep(bool sprite_fetch)
{
    // if rendering is disabled nothing in this substep matters
    if(!rendering_enabled) return 0;

    // phase 1 needs the shift register fully shifted 8 times
    // shift registers start shifting at cycle 2, and the first latch of the shift register happens at cycle 9
    // so we can be sure (at cycles 2, 3, 4, 5, 6, 7, 8, and 9) 8 bits are shifted out before the latch at cycle 9
    if(cycle >= 2 && cycle <= 337) Shift();

    // Look over OAM and prepare sprites
    EvaluateSprites();

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

        if(!sprite_fetch) vram_address = 0x2000 | (vram_address_v & 0x0FFF);
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
        int offset = vram_address_v & 0x3FF;

        // 32 tiles per row, 4 x-tiles represented per attribute byte
        // every 32 * 4 y-tiles = 0x80 tiles, increment attribute table address by 8 bytes 
        // (8 attribute bytes per 0x80 tiles)
        int attribute_addr = (offset & 0x380) >> 4;  // equiv: (offset >> 7) << 3;

        // and add in one for every 4 tiles in the x direction. 0x20 tiles per row, divided by 4
        attribute_addr += (offset & 0x1F) >> 2;

        // and then add the base of the attribute table
        if(!sprite_fetch) vram_address = 0x23C0 | (vram_address_v & 0x0C00) | attribute_addr;
        break;
    }

    case 4:
        // latch attribute byte
        attribute_latch = Read(vram_address);
        break;

    case 5:
        if(!sprite_fetch) {
            // setup lsbits tile address
            int fine_y = (vram_address_v & 0x7000) >> 12;
            // TODO maybe background_pattern_table_address needs special latching
            vram_address = (background_pattern_table_address << 12) | (nametable_latch << 4) | fine_y;
        }
        break;

    case 6:
        // latch lsbits tile byte
        if(sprite_fetch) {
            int sprite = (secondary_oam_address >> 2) - 1; // secondary_oam_address is pointing to the next sprite at this point
            sprite_lsbits[sprite] = Read(vram_address);
        } else {
            background_lsbits_latch = Read(vram_address);
        }
        break;

    case 7:
        // setup msbits tile address, correct for both sprites and tiles
        vram_address += 8;
        break;

    case 0:
        // latch msbits tile byte
        if(sprite_fetch) {
            int sprite = (secondary_oam_address >> 2) - 1; // secondary_oam_address is pointing to the next sprite at this point
            sprite_msbits[sprite] = Read(vram_address);
        } else {
            background_msbits_latch = Read(vram_address);

            // increment 1 tile in X and wrap to the next nametable
            if((vram_address_v & 0x1F) == 0x1F) { // check wrap X to the horizontal nametable
                vram_address_v &= ~0x1F; 
                vram_address_v ^= 0x400;
            } else {
                vram_address_v += 0x01;
            }
        }
        break;
    }

    return DeterminePixel();
}

void PPU::Shift()
{
    background_lsbits <<= 1;
    background_msbits <<= 1;

    // sprites are only shifted during rendering
    if(cycle < 257) {
        for(int sprite = 0; show_sprites && sprite < 8; sprite++) {
            if(sprite_x[sprite] == 0) {
                if(sprite_attribute[sprite] & 0x40) { // flip_x shifts the other direction
                    sprite_lsbits[sprite] >>= 1;
                    sprite_msbits[sprite] >>= 1;
                } else {
                    sprite_lsbits[sprite] <<= 1;
                    sprite_msbits[sprite] <<= 1;
                }
            } else if(sprite_x[sprite] != 0xFF) {
                sprite_x[sprite]--;
            }
        }
    }
}

void PPU::EvaluateSprites()
{
    bool odd_cycle = (cycle & 1) != 0;

    // alternate between accessing the primary and secondary OAM RAM 
    // only up until hblank, at which point we want to read secondary every cycle
    if(odd_cycle && cycle <= 256) {
        if(primary_oam_rw) primary_oam[primary_oam_address] = primary_oam_data;
        else               primary_oam_data = primary_oam[primary_oam_address];
    } else {
        // perform the secondary OAM RAM access
        if(secondary_oam_rw) secondary_oam[secondary_oam_address] = secondary_oam_data;
        else                 secondary_oam_data = secondary_oam[secondary_oam_address];
    }

    // perform different tasks in the scanline depending on what cycle we're in
    if(cycle <= 64) { // Cycles 1-64 clear secondary OAM to $FF
        secondary_oam_rw = 1;
        secondary_oam_data = 0xFF;

        // incrementing this on the first cycle means that secondary OAM will be filled out
        // 1..31 and back to 0. but that leaves the address at 0, which is what we want
        if(odd_cycle) {
            secondary_oam_address = (secondary_oam_address + 1) & 0x1F; // need to support wrapping, as we rely on this being 5 bits
        }
    } else if(cycle <= 256) { // Sprite evaluation
        int sprite_phase = (primary_oam_address - primary_oam_address_bug) & 3;
        switch(sprite_phase) { // depending on which byte of the OAM sprite we have read
        case 0: // read Y byte, copy it to secondary OAM
            if(odd_cycle) { // new data is availble, copy it to secondary OAM 
                secondary_oam_data = primary_oam_data;
                // address is already set up
                // do not touch secondary_oam_rw, as writes become reads when secondary oam is full
            } else {
                // data is written, determine if we should continue writing
                int delta_y = (int)scanline - (int)secondary_oam_data;
                int sz = sprite_size ? 16 : 8;
                if(delta_y >= 0 && delta_y < sz) {
                    // if secondary_oam_rw is set to READ then we've overflowed secondary OAM
                    // but we still continue reading the next 3 bytes from primary OAM
                    if(!secondary_oam_rw) sprite_overflow = 1;

                    // if this is sprite 0, note it as included in the output list
                    if(secondary_oam_rw && (primary_oam_address >> 2) == 0) sprite_zero_next_present = 1;

                    // set up read address for the next data, this triggers a full copy of the sprite
                    primary_oam_address += 1;

                } else {
                    // if secondary OAM is full and this sprite is not displayed, the PPU should advance to
                    // the next sprite, but instead it also increments the sub-sprite data to read first
                    // the tile index, then attributes, then X, and then we essentially skip a sprite
                    // I'll simulate this using the primary_oam_address_bug line which we need to 
                    // make our switch statement think the next byte is actually the Y value
                    if(!secondary_oam_rw) {
                        primary_oam_address += 1;
                        primary_oam_address_bug += 1;
                    }

                    // sprite is not within range, so read the next sprite from primary
                    primary_oam_address += 4;
                }

                // if primary_oam_address overflows, we disable writes to secondary OAM as well
                if(primary_oam_address == 0) secondary_oam_rw = 0;
            }
            break;

        // the next 3 bytes of the sprite are simply copied from primary to secondary
        case 1:
        case 2:
        case 3:
            if(odd_cycle) {
                // data from primary OAM is ready, write it to next address in secondary OAM
                secondary_oam_address = (secondary_oam_address + 1) & 0x1F;
                secondary_oam_data = primary_oam_data;
            } else {
                // data was written, prepare to read the next byte
                primary_oam_address += 1;

                // if primary_oam_address overflows, we disable writes to secondary OAM as well
                if(primary_oam_address == 0) secondary_oam_rw = 0;

                // after writing the X byte we need to move onto the next sprite on secondary 
                if(sprite_phase == 3) secondary_oam_address = (secondary_oam_address + 1) & 0x1F;
            }

            // if secondary_oam_address overflows, we have no more room for sprite data, so change to reads
            if(secondary_oam_address == 0) secondary_oam_rw = 0;
            break;
        }

        // reset secondary_oam lines going into hblank
        if(cycle == 256) {
            secondary_oam_rw = 0;
            secondary_oam_address = 0;
        }
    } else if(cycle <= 320) { // now in hblank (cycles 257-320), so we need to fetch OAM data and tiles
        int sprite = (secondary_oam_address >> 2);
        switch(cycle & 7) {
        case 1:
            // latch delta Y coordinate into vram_address, since it's used to select which row of the tile we want to read
            // InternalStep() won't overwrite vram_address in hblank
            vram_address = scanline - secondary_oam_data;
            if(vram_address >= 8) { // only happens when sprite_size is 8x16
                // bottom half of tile is 16 bytes away, but 8 bytes are accounted for in the Y position
                vram_address += 0x08;
            }
            // increment address to read the tile index
            secondary_oam_address++;
            break;
        case 2:
            // set vram_address to point at the tile data
            if(sprite_size) {
                // low bit of tile number picks bank $0000 or $1000
                int base = (secondary_oam_data & 1) << 12;
                // and tiles are 32 bytes long
                vram_address |= base | ((secondary_oam_data & 0xFE) << 4);
            } else {
                // add tile * 16 to vram_address
                vram_address |= (sprite_pattern_table_address << 12) | (secondary_oam_data << 4);
            }
            // increment address to read sprite attribute byte
            secondary_oam_address++;
            break;
        case 3:
            // latch attribute
            sprite_attribute[sprite] = secondary_oam_data;

            // when sprites are flipped vertically, we have to change the row of pixels we fetch
            if(sprite_attribute[sprite] & 0x80) {
                if(sprite_size) {
                    // TODO this is probably not right..
                    int cur_y = (vram_address & 0x07) + ((vram_address & 0x10) ? 8 : 0);
                    int new_y = 15 - cur_y;
                    vram_address = (vram_address & ~0x1F) | (new_y & 0x07) | ((new_y & 0x08) << 1);
                } else {
                    vram_address = (vram_address & ~0x07) | (~(vram_address & 0x07) & 0x07);
                }
            }

            // increment address to fetch sprite x coordinate
            secondary_oam_address++;
            break;
        case 4:
            // latch x coordinate. empty OAM slots have X coordinate at 0xFF
            sprite_x[sprite] = secondary_oam_data;

            // increment address to point to next sprite's Y coordinate
            secondary_oam_address++;
            break;
        default:
            // cycles 5,6,7,0 wait for tile data to be read
            break;
        }

        // primary OAM address is initialized to read address 0 in preparation for evaluation
        primary_oam_rw = 0;
        primary_oam_address = 0;
        primary_oam_address_bug = 0;
    } else if(cycle <= 340) {
        // setup address to read the first byte of secondary OAM
        secondary_oam_rw = 0;
        secondary_oam_address = 0;
    }

    // Sprite hit only triggers after 2nd cycle
    if(sprite_zero_hit_buffer && cycle >= 2) sprite0_hit = 1;
}

int PPU::DeterminePixel()
{
    int tile_color; // 0, 1, 2, or 3
    int background_color = DetermineBackgroundColor(tile_color);

    // iterate over sprites, looking for active sprites
    int sprite_priority = 0;   // 0 = foreground priority
    int sprite_tile_color = 0; // 0 (transparent), 1, 2, or 3
    int sprite_color = 0;      // color from palette ram
    int sprite = 0;
    for(; show_sprites && sprite < 8; sprite++) {
        if(sprite_x[sprite] != 0) continue;

        // determine sprite's color
        int flip_x = (sprite_attribute[sprite] & 0x40);
        int bit0 = flip_x ? (sprite_lsbits[sprite] & 0x01) : ((sprite_lsbits[sprite] & 0x80) >> 7);
        int bit1 = flip_x ? (sprite_msbits[sprite] & 0x01) : ((sprite_msbits[sprite] & 0x80) >> 7);
        int attr = (sprite_attribute[sprite] & 0x03);
        sprite_tile_color = (bit1 << 1) | bit0;
        if(sprite_tile_color != 0) {
            int palette_index = (attr << 2) | sprite_tile_color;
            sprite_color = palette_ram[0x10 | palette_index];
            sprite_priority = (sprite_attribute[sprite] & 0x20) >> 5;
            break;
        }
    }

    // select background or sprite color based on sprite priority
    int mux_color = background_color;
    if(sprite_tile_color != 0) {
        // if sprite has priority or the background is color 0, use the sprite color
        if((sprite_priority == 0) || (tile_color == 0)) mux_color = sprite_color;

        // sprite0 hit flag happens when a non-zero pixel of sprite 0 covers a nonzero pixel of the background
        // sprite0 hit flag doesn't mean the sprite color is used, just when the two colors appear at the same time
        if(sprite_zero_present && (sprite == 0) && (tile_color != 0)) sprite_zero_hit_buffer = 1;
    } 

    return rgb_palette_map[mux_color];
}

// DeterminePixel has no side effects
// Rendering a pixel seems so easy when all the hard work of determining addresses and shifts
// is done beforehand
int PPU::DetermineBackgroundColor(int& tile_color) const
{
    // determine tile color (2 bit)
    u8 bit0 = (u8)((background_lsbits >> (15 - fine_x)) & 0x01);
    u8 bit1 = (u8)((background_msbits >> (15 - fine_x)) & 0x01);
    tile_color = ((bit1 << 1) | bit0);

    // determine attribute bits (palette index), 2 bits
    //
    // if fine_x puts us into the next tile, change the attribute byte to the next and possibly adjust which 2 bits to use
    // (x_pos is two tiles ahead due to the prefetching of tiles)
    // (first pixel is produced at cycle==1, so subtract 17 total)
    int x_pos = (cycle & 0x07) + (((vram_address_v & 0x1F) << 3) | (int)fine_x);
    int attr_x = (x_pos - 17) & 0x1F; // the actual 2 bits to use is determined by the x coordinate in the 0..31 pixel block

    // if the current fine x position plus scroll fine_x puts us into rendering the next tile (which is in the shift buffer)
    // then we also need to use the next attribute byte. note that attr_x will have looped back around 32 pixels
    int actual_attribute_byte = ((((cycle - 1) & 0x07) + fine_x) >= 8) ? attribute_next_byte : attribute_byte;

    // left/right nibble switches on y_pos every 16 rows
    int y_pos = ((vram_address_v & 0x3E0) >> 2) | ((vram_address_v & 0x7000) >> 12);
    int y_shift = (y_pos & 0x10) >> 2; // y_shift = 0 or 4

    // left/right two-bits of said nibble switches on whether attr_x >= 16
    int x_shift = (attr_x & 0x10) >> 3; // x_shift is 0 or 2
    int attr = (actual_attribute_byte >> (y_shift + x_shift)) & 0x03;

    // NES palette lookup is 4 bits/16 colors
    u8  nes_palette_index = ((attr << 2) | tile_color);

    // translate palette + tile_color into RGB
    int nes_color = palette_ram[tile_color == 0 ? 0 : nes_palette_index];

    return nes_color & 0x3F;
}

}
