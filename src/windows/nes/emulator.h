#pragma once

#include <memory>
#include <stack>
#include <thread>

#include "signals.h"
#include "systems/nes/nes_cpu.h"
#include "systems/nes/nes_system.h"
#include "windows/basewindow.h"

namespace NES {

class GlobalMemoryLocation;
class PPU;

namespace Windows {

class Emulator : public BaseWindow {
public:
    enum class State {
        INIT,
        PAUSED,
        RUNNING,
        STEP_CYCLE,
        STEP_INSTRUCTION,
        CRASHED
    };

    Emulator();
    virtual ~Emulator();

    virtual char const * const GetWindowClass() { return Emulator::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::Emulator"; }
    static std::shared_ptr<Emulator> CreateWindow();

    // signals

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;
    void CheckInput() override;

private:
    void Reset();
    void UpdateRAMTexture();
    void UpdatePPUTexture();
    void UpdateNametableTexture();
    bool SingleCycle();
    void StepPPU();
    void EmulationThread();

    std::weak_ptr<System>        current_system;
    State                        current_state = State::INIT;
    std::shared_ptr<std::thread> emulation_thread;
    bool                         exit_thread = false;
    bool                         thread_exited = false;
    std::shared_ptr<CPU>         cpu;
    std::shared_ptr<PPU>         ppu;
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
};

} //namespace Windows

} //namespace NES
