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

namespace Windows {

class Emulator : public BaseWindow {
public:
    enum class State {
        INIT,
        PAUSED,
        RUNNING,
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
    void EmulationThread();

    std::weak_ptr<System>        current_system;
    State                        current_state = State::INIT;
    std::shared_ptr<std::thread> emulation_thread;
    bool                         exit_thread = false;
    bool                         thread_exited = false;
    std::shared_ptr<CPU>         cpu;
};

} //namespace Windows

} //namespace NES
