// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <functional>
#include <memory>

namespace Systems::NES {

class MemoryView;
class PPUView;

extern int const rgb_palette_map[];

class PPU : public std::enable_shared_from_this<PPU> {
public:
    typedef std::function<void(int)> nmi_function_t;

    typedef std::function<u8(u16)> read_func_t;
    typedef std::function<void(u16, u8)> write_func_t;

    PPU(nmi_function_t const&, read_func_t const& peek_func, read_func_t const& read_func, write_func_t const& write_func);
    ~PPU();

    void Reset();

    inline int GetCycle() const { return cycle; }
    inline int GetScanline() const { return scanline; }
    inline int GetFrame() const { return frame; }
    inline int GetIsOdd() const { return odd; }
    inline u8 GetPPUCONT() const { return ppucont; }
    inline u8 GetPPUMASK() const { return ppumask; }
    inline u8 GetPPUSTAT() const { return ppustat; }
    inline u16 GetVramAddress() const { return vram_address; }
    inline u16 GetVramAddressT() const { return vram_address_t; }
    inline u16 GetVramAddressV() const { return vram_address_v; }
    inline u16 GetScrollX() const { return scroll_x; }
    inline u16 GetScrollY() const { return scroll_y; }

    inline void CopyOAM(u8* dest) {
        memcpy(dest, primary_oam, sizeof(primary_oam));
    }

    inline void CopyPaletteRAM(u8* dest, bool sprites) {
        memcpy(dest, sprites ? &palette_ram[0x10] : palette_ram, 0x10);
    }

    // returns color, outputs true for either blanking period
    int Step(bool& hblank_out, bool& vblank_out);

    std::shared_ptr<MemoryView> CreateMemoryView();
private:
    int  InternalStep(bool);
    void Shift();
    void EvaluateSprites();
    int  DeterminePixel();
    int  DetermineBackgroundColor(int&) const;

    union {
        u8 ppucont;
        struct {
            u8 base_nametable_address           : 2;
            u8 vram_increment                   : 1;
            u8 sprite_pattern_table_address     : 1;
            u8 background_pattern_table_address : 1;
            u8 sprite_size                      : 1;
            u8 _master_slave                    : 1; // unused
            u8 enable_nmi                       : 1;
        };
    };

    union {
        u8 ppumask;
        struct {
            u8 greyscale             : 1;
            u8 show_background_left8 : 1;
            u8 show_sprites_left8    : 1;
            u8 show_background       : 1;
            u8 show_sprites          : 1;
            u8 emphasize_red         : 1;
            u8 emphasize_green       : 1;
            u8 emphasize_blue        : 1;
        };
    };

    union {
        u8 ppustat;
        struct {
            u8 unused0         : 5;
            u8 sprite_overflow : 1;
            u8 sprite0_hit     : 1;
            u8 vblank          : 1;
        };
    };

    bool rendering_enabled;
    bool prevent_nmi_this_frame;

    // NMI wire connected directly to the CP
    nmi_function_t nmi;

    // loopy vars.. they're honestly a decent way of properly timing all these changes
    // but I wanted to keep individual vars up to date. instead, I'll track t (temp address)
    // and v (vram address) like everyone else.. "vram_address" is the final address that ends
    // up on the PPU address bus the cycle before the read
    // see https://www.nesdev.org/wiki/PPU_scrolling
    u16 vram_address;
    u16 vram_address_t;
    u16 vram_address_v;
    u8  fine_x;

    // read buffer on the PPUDATA port
    u8  vram_read_buffer;

    // toggle on PPUADDR and PPUSCRL
    int vram_address_latch;

    // PPU bus (the System module handles the VRAM connection)
    read_func_t Peek;
    read_func_t Read;
    write_func_t Write;

    // internal counting registers
    int frame;
    int scanline;
    int cycle;
    int odd;

    // used only in the debugger
    int scroll_x;
    int scroll_y;

    // color pipeline, color produced at cycle 2 is generated at cycle 4
    int color_pipeline[3];
    
    // incoming data latches
    u8 nametable_latch;
    u8 attribute_latch;
    u8 background_lsbits_latch;
    u8 background_msbits_latch;

    // shift registers for background
    u8  attribute_next_byte;
    u8  attribute_byte;
    u16 background_lsbits;
    u16 background_msbits;

    // primary and secondary OAM ram
    u8 primary_oam[256];
    u8 primary_oam_rw; // 1= write
    u8 primary_oam_address;         // also the address used in port $2003
    u8 primary_oam_address_bug;
    u8 primary_oam_data;
    u8 secondary_oam[32];
    u8 secondary_oam_rw; // 1= write
    u8 secondary_oam_address;
    u8 secondary_oam_data;

    // the rendering sprite states for the current scanline
    u8 sprite_lsbits[8];
    u8 sprite_msbits[8];
    u8 sprite_attribute[8];
    u8 sprite_x[8];

    // for tracking sprite 0 hit
    struct {
        u8 unused1                  : 5;
        u8 sprite_zero_present      : 1;
        u8 sprite_zero_next_present : 1;
        u8 sprite_zero_hit_buffer   : 1;
    };

    // palette RAM, 16 bytes for BG, 16 for OAM
    u8 palette_ram[0x20];

    friend class PPUView;
};

}
