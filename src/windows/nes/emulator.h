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
}

namespace Windows::NES {

class Listing;

// NES::Windows::System is home to everything you need about an instance of a NES system.  
// You can have multiple System windows, and that contains its own system state. 
// NES::System is generic and doesn't contain instance specific state. That information
// is designated to be here
class SystemInstance : public BaseWindow {
public:
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

    // signals

protected:
    void CheckInput() override;
    void Update(double deltaTime) override;
    void Render() override;

private:
    void ChildWindowAdded(std::shared_ptr<BaseWindow> const&);
    void ChildWindowRemoved(std::shared_ptr<BaseWindow> const&);

    void UpdateTitle();
    void Reset();
    void UpdateRAMTexture();
    void UpdatePPUTexture();
    void UpdateNametableTexture();
    bool SingleCycle();
    bool StepCPU();
    void StepPPU();
    void EmulationThread();
    void WriteOAMDMA(u8);

    static int  next_system_id;
    int         system_id;
    std::string system_title;

    std::shared_ptr<BaseWindow>           most_recent_listing_window;

    std::weak_ptr<Systems::NES::System>   current_system;
    State                                 current_state = State::INIT;
    std::shared_ptr<std::thread>          emulation_thread;
    bool                                  exit_thread = false;
    bool                                  thread_exited = false;
    std::shared_ptr<Systems::NES::CPU>    cpu;
    std::shared_ptr<Systems::NES::PPU>    ppu;
    std::shared_ptr<Systems::NES::APU_IO> apu_io;

    std::shared_ptr<Systems::NES::MemoryView> memory_view;

    std::string run_to_address_str = "";
    int         run_to_address = -1;
    int         cpu_shift;

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

} //namespace Windows::NES
