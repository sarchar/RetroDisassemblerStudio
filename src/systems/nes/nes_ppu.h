#pragma once

#include <functional>
#include <memory>

namespace NES {

class MemoryView;
class PPUView;

class PPU : public std::enable_shared_from_this<PPU> {
public:
    typedef std::function<void()> nmi_function_t;
    PPU(nmi_function_t const&);
    ~PPU();

    void Reset();
    void Step();

    std::shared_ptr<MemoryView> CreateMemoryView();
private:
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

    friend class PPUView;
};

}
