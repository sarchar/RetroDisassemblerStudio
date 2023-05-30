#pragma once

#include <memory>
#include <stack>
#include <thread>

#include "signals.h"
#include "windows/basewindow.h"

// GetMySystemInstance only available in some windows
#define GetMySystemInstance() this->GetParentWindowAs<Windows::NES::SystemInstance>()

// return the most recent 
#define GetMyListing() (GetMySystemInstance() ? GetMySystemInstance()->GetMostRecentListingWindow() : nullptr)

namespace Systems::NES {
    class APU_IO;
    class CPU;
    class GlobalMemoryLocation;
    class PPU;
    class MemoryView;
    class System;
}

namespace Windows::NES {

class Listing;

// Windows::NES::SystemInstance is home to everything you need about an instance of a NES system.  
// You can have multiple SystemInstances and they contain their own system state. 
// Systems::NES::System is generic and doesn't contain instance specific state
class SystemInstance : public BaseWindow {
public:
    using APU_IO     = Systems::NES::APU_IO;
    using CPU        = Systems::NES::CPU;
    using MemoryView = Systems::NES::MemoryView;
    using PPU        = Systems::NES::PPU;
    using System     = Systems::NES::System;

    enum class State {
        INIT,
        PAUSED,
        RUNNING,
        STEP_CYCLE,
        STEP_INSTRUCTION,
        CRASHED
    };

    SystemInstance();
    virtual ~SystemInstance();

    virtual char const * const GetWindowClass() { return SystemInstance::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::SystemInstance"; }
    static std::shared_ptr<SystemInstance> CreateWindow();

    // create a default workspace
    void CreateDefaultWorkspace();
    void CreateNewWindow(std::string const&);

    std::shared_ptr<Listing> GetMostRecentListingWindow() const {
        return dynamic_pointer_cast<Listing>(most_recent_listing_window);
    }


    std::shared_ptr<APU_IO>     const& GetAPUIO()      { return apu_io; }
    std::shared_ptr<CPU>        const& GetCPU()        { return cpu; }
    u32                         const* GetFramebuffer() const { return framebuffer; }
    std::shared_ptr<MemoryView> const& GetMemoryView() { return memory_view; }

    // signals

protected:
    void RenderMenuBar() override;
    void CheckInput() override;
    void Update(double deltaTime) override;
    void Render() override;

private:
    void ChildWindowAdded(std::shared_ptr<BaseWindow> const&);
    void ChildWindowRemoved(std::shared_ptr<BaseWindow> const&);

    void UpdateTitle();
    void Reset();
    void UpdateRAMTexture();
    void UpdateNametableTexture();
    bool SingleCycle();
    bool StepCPU();
    void StepPPU();
    void EmulationThread();
    void WriteOAMDMA(u8);

    static int  next_system_id;
    int         system_id;
    std::string system_title;

    std::shared_ptr<BaseWindow>  most_recent_listing_window;

    std::shared_ptr<System>      current_system;
    State                        current_state = State::INIT;
    bool                         running = false;
    std::shared_ptr<std::thread> emulation_thread;
    bool                         exit_thread = false;
    bool                         thread_exited = false;
    std::shared_ptr<CPU>         cpu;
    std::shared_ptr<PPU>         ppu;
    std::shared_ptr<APU_IO>      apu_io;

    std::shared_ptr<MemoryView> memory_view;

    std::string run_to_address_str = "";
    int         run_to_address = -1;
    int         cpu_shift = 0;

    u64 last_cycle_count = 0;
    std::chrono::time_point<std::chrono::steady_clock> last_cycle_time;
    double cycles_per_sec;

    // Framebuffers are 0xAABBGGRR format (MSB = alpha)
    u32*                         framebuffer;
    u32*                         ram_framebuffer;
    u32*                         nametable_framebuffer;

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

class Screen : public BaseWindow {
public:
    Screen();
    virtual ~Screen();

    virtual char const * const GetWindowClass() { return Screen::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::Screen"; }
    static std::shared_ptr<Screen> CreateWindow();

protected:
    void CheckInput() override;
    void Update(double deltaTime) override;
    void PreRender() override;
    void Render() override;

private:
    void* framebuffer_texture;
};
 
class CPUState : public BaseWindow {
public:
    CPUState();
    virtual ~CPUState();

    virtual char const * const GetWindowClass() { return CPUState::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::CPUState"; }
    static std::shared_ptr<CPUState> CreateWindow();

protected:
    void CheckInput() override;
    void Update(double deltaTime) override;
    void Render() override;

private:
    void* framebuffer_texture;
};

} //namespace Windows::NES
