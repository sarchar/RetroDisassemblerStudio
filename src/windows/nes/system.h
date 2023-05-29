#pragma once

#include <memory>
#include <stack>
#include <thread>

#include "signals.h"
#include "systems/nes/nes_system.h"
#include "windows/basewindow.h"

namespace NES {

namespace Windows {

// NES::Windows::System is home to everything you need about an instance of a NES system.  
// You can have multiple System windows, and that contains its own system state. 
// NES::System is generic and doesn't contain instance specific state. That information
// is designated to be here
class System : public BaseWindow {
public:
    System();
    virtual ~System();

    virtual char const * const GetWindowClass() { return System::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::System"; }
    static std::shared_ptr<System> CreateWindow();

    // signals

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;
    void CheckInput() override;

private:
    std::weak_ptr<NES::System>   current_system;
    State                        current_state = State::INIT;
    std::shared_ptr<std::thread> emulation_thread;
    bool                         exit_thread = false;
    bool                         thread_exited = false;
    std::shared_ptr<CPU>         cpu;
    std::shared_ptr<PPU>         ppu;
    std::shared_ptr<APU_IO>      apu_io;
    std::shared_ptr<MemoryView>  memory_view;

    std::string                  run_to_address_str = "";
    int                          run_to_address = -1;
    int                          cpu_shift;

    u64 last_cycle_count = 0;
    std::chrono::time_point<std::chrono::steady_clock> last_cycle_time;
    double cycles_per_sec;

    // Framebuffers are 0xAABBGGRR format (MSB = alpha)
    u32*                         framebuffer;
    u32*                         ram_framebuffer;
    u32*                         nametable_framebuffer;

    void*                        framebuffer_texture;
    void*                        ram_texture;
    void*                        nametable_texture;

    // rasterizer position
    bool                         hblank;
    u32*                         raster_line;
    int                          raster_y;

    // OAM DMA
    bool                         oam_dma_enabled = false;
    u16                          oam_dma_source;
    u8                           oam_dma_rw;
    u8                           oam_dma_read_latch;
    bool                         dma_halt_cycle_done;

    signal_connection            oam_dma_callback_connection;
};

} //namespace Windows

} //namespace NES
