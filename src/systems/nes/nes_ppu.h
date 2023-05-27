#pragma once

#include <functional>
#include <memory>

namespace NES {

class MemoryView;
class PPUView;

class PPU : public std::enable_shared_from_this<PPU> {
public:
    typedef std::function<void()> nmi_function_t;

    typedef std::function<u8(u16)> read_func_t;
    typedef std::function<void(u16, u8)> write_func_t;

    PPU(nmi_function_t const&, read_func_t const& read_func, write_func_t const& write_func);
    ~PPU();

    void Reset();

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
            u8 unused0         : 4;
            u8 sprite_overflow : 1;
            u8 sprite0_hit     : 1;
            u8 vblank          : 1;
        };
    };

    // NMI wire connected directly to the CP
    nmi_function_t nmi;

    // internal scroll registers
    u8  scroll_x;
    u8  scroll_y;
    u8  scroll_y_latch;

    // the PPU bus address to use with Read/Write
    u16 vram_address;
    u8  vram_read_buffer;
    int vram_address_latch;

    // PPU bus (the System module handles the VRAM connection)
    read_func_t Read;
    write_func_t Write;

    // internal counting registers
    int scanline;
    int cycle;
    int odd;

    // moving x/y positions for the calculation of nametable and attribute bytes
    // y_pos includes scroll_y, x_pos does not
    int x_pos;
    int y_pos;

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
        u8 unused1                : 6;
        u8 sprite_zero_present    : 1;
        u8 sprite_zero_hit_buffer : 1;
    };

    // palette RAM, 16 bytes for BG, 16 for OAM
    u8 palette_ram[0x20];

    friend class PPUView;
};

}
