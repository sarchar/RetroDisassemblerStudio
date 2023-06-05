// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <memory>
#include <stack>
#include <thread>
#include <unordered_map>
#include <variant>

#include "signals.h"
#include "systems/nes/memory.h"
#include "windows/basewindow.h"

// GetMySystemInstance only available in some windows
#define GetMySystemInstance() this->GetParentWindowAs<Windows::NES::SystemInstance>()

// return the most recent 
#define GetMyListing() (GetMySystemInstance() ? GetMySystemInstance()->GetMostRecentListingWindow() : nullptr)

class BaseExpression;
class BaseExpressionNode;

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

struct BreakpointInfo {
    Systems::NES::GlobalMemoryLocation address;
    bool enabled = true;
    bool has_bank = false; // true when prg/chr_rom_bank in address is valid

    std::shared_ptr<BaseExpression> condition;

    // these could be a single int but separate bools work better with ImGui
    bool break_read = false;
    bool break_write = false;
    bool break_execute = false;

    bool Save(std::ostream&, std::string&) const;
    bool Load(std::istream&, std::string&);

    bool operator==(BreakpointInfo const& other) {
        return address == other.address;
    }
};

// Windows::NES::SystemInstance is home to everything you need about an instance of a NES system.  
// You can have multiple SystemInstances and they contain their own system state. 
// Systems::NES::System is generic and doesn't contain instance specific state
class SystemInstance : public BaseWindow {
public:
    using APU_IO               = Systems::NES::APU_IO;
    using CPU                  = Systems::NES::CPU;
    using GlobalMemoryLocation = Systems::NES::GlobalMemoryLocation;
    using MemoryView           = Systems::NES::MemoryView;
    using PPU                  = Systems::NES::PPU;
    using System               = Systems::NES::System;

    typedef std::variant<GlobalMemoryLocation, u16> breakpoint_key_t;
    typedef std::vector<std::shared_ptr<BreakpointInfo>> breakpoint_list_t;

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
    static char const * const GetWindowClassStatic() { return "Windows::NES::SystemInstance"; }
    static std::shared_ptr<SystemInstance> CreateWindow();

    std::string const& GetInstanceName() const { return instance_name; }

    // create a default workspace
    void CreateDefaultWorkspace();
    void CreateNewWindow(std::string const&);

    std::shared_ptr<Listing> GetMostRecentListingWindow() const {
        return dynamic_pointer_cast<Listing>(most_recent_listing_window);
    }


    std::shared_ptr<APU_IO>     const& GetAPUIO()      { return apu_io; }
    std::shared_ptr<CPU>        const& GetCPU()        { return cpu; }
    std::shared_ptr<PPU>        const& GetPPU()        { return ppu; }
    u32                         const* GetFramebuffer() const { return framebuffer; }
    std::shared_ptr<MemoryView> const& GetMemoryView() { return memory_view; }
    void GetCurrentInstructionAddress(GlobalMemoryLocation*);

    inline void SetBreakpoint(breakpoint_key_t const& key, std::shared_ptr<BreakpointInfo> const& breakpoint_info) {
        breakpoints[key].push_back(breakpoint_info);
        // set cpu_quick_breakpoints bit
        cpu_quick_breakpoints[breakpoint_info->address.address >> 5] |= (1 << (breakpoint_info->address.address & 0x1F));
    }

    inline void ClearBreakpoint(breakpoint_key_t const& key, std::shared_ptr<BreakpointInfo> const& breakpoint_info) {
        if(!breakpoints.contains(key)) return;

        auto breakpoint_list = breakpoints[key];
        auto it = std::find(breakpoint_list.begin(), breakpoint_list.end(), breakpoint_info);
        if(it == breakpoint_list.end()) return;

        breakpoint_list.erase(it);
        if(breakpoint_list.size() != 0) return;

        breakpoints.erase(key);

        // unset cpu_quick_breakpoints bit when there are no bp at this address
        cpu_quick_breakpoints[breakpoint_info->address.address >> 5] &= ~(1 << (breakpoint_info->address.address & 0x1F));
    }

    inline breakpoint_list_t const& GetBreakpointsAt(breakpoint_key_t const& where) {
        static SystemInstance::breakpoint_list_t empty_list;
        if(breakpoints.contains(where)) return breakpoints[where];
        return empty_list;
    }

    bool SetBreakpointCondition(std::shared_ptr<BreakpointInfo> const&, std::shared_ptr<BaseExpression> const&, std::string&);

    template<typename T>
    inline void IterateBreakpoints(T const& func) {
        for(auto& bplistpair : breakpoints) {
            for(auto& bpi : bplistpair.second) {
                func(bpi);
            }
        }
    }

    enum class CheckBreakpointMode { READ, WRITE, EXECUTE };
    void CheckBreakpoints(u16 address, CheckBreakpointMode mode);

    // signals
    // be careful using breakpoint_hit signal, as it's emitted from the emulation thread and not the main
    // render thread. you should only set a flag and check the state of that flag in the main render thread
    typedef signal<std::function<void(std::shared_ptr<BreakpointInfo> const&)>> breakpoint_hit_t;
    std::shared_ptr<breakpoint_hit_t> breakpoint_hit;

protected:
    void RenderMenuBar() override;
    void CheckInput() override;
    void Update(double deltaTime) override;
    void Render() override;

    bool SaveWindow(std::ostream&, std::string&) override;
    bool LoadWindow(std::istream&, std::string&) override;

private:
    void ChildWindowAdded(std::shared_ptr<BaseWindow> const&);
    void ChildWindowRemoved(std::shared_ptr<BaseWindow> const&);

    void UpdateTitle();
    void Reset();
    void UpdateRAMTexture();
    bool SingleCycle();
    bool StepCPU();
    void StepPPU();
    void EmulationThread();
    void WriteOAMDMA(u8);

    static int  next_system_id;
    int         system_id;
    std::string system_title;
    std::string instance_name;

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

    bool        step_instruction_done = false;
    int         cpu_shift = 0;

    u64 last_cycle_count = 0;
    std::chrono::time_point<std::chrono::steady_clock> last_cycle_time;
    double cycles_per_sec;

    // Framebuffers are 0xAABBGGRR format (MSB = alpha)
    u32*                         framebuffer;
    u32*                         ram_framebuffer;

    void*                        ram_texture;

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

    // breakpoints
    std::unordered_map<breakpoint_key_t, breakpoint_list_t> breakpoints;
    u32* cpu_quick_breakpoints;
};

class Screen : public BaseWindow {
public:
    Screen();
    virtual ~Screen();

    virtual char const * const GetWindowClass() { return Screen::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "Windows::NES::Screen"; }
    static std::shared_ptr<Screen> CreateWindow();

protected:
    void CheckInput() override;
    void Update(double deltaTime) override;
    void PreRender() override;
    void Render() override;

private:
    void* framebuffer_texture;
    bool  valid_texture = false;
};
 
class CPUState : public BaseWindow {
public:
    CPUState();
    virtual ~CPUState();

    virtual char const * const GetWindowClass() { return CPUState::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "Windows::NES::CPUState"; }
    static std::shared_ptr<CPUState> CreateWindow();

protected:
    void CheckInput() override;
    void Update(double deltaTime) override;
    void Render() override;

private:
    void* framebuffer_texture;
};

class PPUState : public BaseWindow {
public:
    using PPU = Systems::NES::PPU;

    PPUState();
    virtual ~PPUState();

    virtual char const * const GetWindowClass() { return PPUState::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "Windows::NES::PPUState"; }
    static std::shared_ptr<PPUState> CreateWindow();

protected:
    void CheckInput() override;
    void Update(double deltaTime) override;
    void Render() override;

    bool SaveWindow(std::ostream&, std::string&) override;
    bool LoadWindow(std::istream&, std::string&) override;

private:
    void RenderRegisters(std::shared_ptr<PPU> const&);
    void RenderNametables(std::shared_ptr<PPU> const&);
    void RenderPalettes(std::shared_ptr<PPU> const&);
    void RenderSprites(std::shared_ptr<PPU> const&);

    int   display_mode = 0;
    bool  show_scroll_window = true;
    int   hovered_palette_index = -1;

    void  UpdateNametableTexture();
    u32*  nametable_framebuffer;
    void* nametable_texture;
    bool  valid_texture = false;
};

class Watch : public BaseWindow {
public:
    Watch();
    virtual ~Watch();

    virtual char const * const GetWindowClass() { return Watch::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "Windows::NES::Watch"; }
    static std::shared_ptr<Watch> CreateWindow();

    void CreateWatch(std::string const&);

protected:
    void CheckInput() override;
    void Update(double deltaTime) override;
    void Render() override;

    bool SaveWindow(std::ostream&, std::string&) override;
    bool LoadWindow(std::istream&, std::string&) override;

private:
    void Resort();
    bool need_resort = false;
    int  sort_column = -1;
    int  selected_row = -1;
    bool reverse_sort = false;

    void SetWatch();
    int  editing = -1; // -1 means not editing  
    bool started_editing = false;
    bool do_set_watch = false;
    bool wait_dialog = false;
    std::string edit_string;
    std::string set_watch_error_message;

    bool ExploreCallback(std::shared_ptr<BaseExpressionNode>&, std::shared_ptr<BaseExpressionNode> const&, int, void*);

    // DereferenceOp functions
    bool DereferenceByte(s64, s64*, std::string&);
    bool DereferenceWord(s64, s64*, std::string&);
    bool DereferenceLong(s64, s64*, std::string&);

    struct WatchData {
        enum class DataType {
            BYTE, WORD, LONG, FLOAT32
        };

        std::shared_ptr<BaseExpression> expression;
        s64                             last_value        = 0;
        DataType                        data_type         = DataType::BYTE;
        bool                            pad               = true;
        int                             base              = 16; // number base for display

        bool Save(std::ostream&, std::string&) const;
        bool Load(std::istream&, std::string&);
    };

    struct ExploreData {
        std::shared_ptr<WatchData> watch_data;
    };

    bool SetDereferenceOp(std::shared_ptr<WatchData> const&);

    std::vector<std::shared_ptr<WatchData>> watches;
    std::vector<int> sorted_watches;
};

class Breakpoints : public BaseWindow {
public:
    Breakpoints();
    virtual ~Breakpoints();

    virtual char const * const GetWindowClass() { return Breakpoints::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "Windows::NES::Breakpoints"; }
    static std::shared_ptr<Breakpoints> CreateWindow();

protected:
    void CheckInput() override;
    void Update(double deltaTime) override;
    void Render() override;

private:
    int selected_row = -1;

    enum class EditMode { NONE, ADDRESS, CONDITION, };

    void SetBreakpoint();
    void SetCondition();

    std::shared_ptr<BreakpointInfo> editing_breakpoint_info;
    std::string edit_string;
    EditMode editing = EditMode::NONE;
    bool started_editing = false;
    bool do_set_breakpoint = false;
    bool wait_dialog = false;
    std::string set_breakpoint_error_message;

    std::shared_ptr<BreakpointInfo> context_breakpoint;
};


} //namespace Windows::NES
