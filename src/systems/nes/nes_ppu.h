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
    int  InternalStep();
    void Shift();
    int  DeterminePixel() const;

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
            u8 unused0 : 7;
            u8 vblank  : 1;
        };
    };

    // NMI wire connected directly to the CP
    nmi_function_t nmi;

    // internal scroll registers
    u8  scroll_x;
    u8  scroll_y;

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

    // palette RAM, 16 bytes for BG, 16 for OAM
    u8 palette_ram[0x20];

    friend class PPUView;
};

}
