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
    void Step();

    std::shared_ptr<MemoryView> CreateMemoryView();
private:
    void InternalStep(int);
    void OutputPixel();

    union {
        u8 ppucont;
        struct {
            int base_nametable_address           : 2;
            int vram_increment                   : 1;
            int sprite_pattern_table_address     : 1;
            int background_pattern_table_address : 1;
            int sprite_size                      : 1;
            int _master_slave                    : 1; // unused
            int enable_nmi                       : 1;
        };
    };

    union {
        u8 ppumask;
        struct {
            int greyscale             : 1;
            int show_background_left8 : 1;
            int show_sprites_left8    : 1;
            int show_background       : 1;
            int show_sprites          : 1;
            int emphasize_red         : 1;
            int emphasize_green       : 1;
            int emphasize_blue        : 1;
        };
    };

    union {
        u8 ppustat;
        struct {
            int unused0 : 7;
            int vblank  : 1;
        };
    };

    nmi_function_t nmi;
    int scanline;
    int cycle;

    u16 vram_address;
    int vram_address_latch;

    read_func_t Read;
    write_func_t Write;

    friend class PPUView;
};

}
