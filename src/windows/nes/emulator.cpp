// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#include <bitset>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <thread>

#include <GL/gl3w.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "magic_enum.hpp"

#include "util.h"

#include "systems/nes/apu_io.h"
#include "systems/nes/cartridge.h"
#include "systems/nes/cpu.h"
#include "systems/nes/disasm.h"
#include "systems/nes/expressions.h"
#include "systems/nes/ppu.h"
#include "systems/nes/system.h"

#include "windows/nes/enums.h"
#include "windows/nes/emulator.h"
#include "windows/nes/defines.h"
#include "windows/nes/labels.h"
#include "windows/nes/listing.h"
#include "windows/nes/project.h"
#include "windows/nes/regions.h"

using namespace std;

using FixupFlags = Systems::NES::FixupFlags;

namespace Windows::NES {

int SystemInstance::next_system_id = 1;

REGISTER_WINDOW(SystemInstance);
REGISTER_WINDOW(Screen);
REGISTER_WINDOW(Watch);
REGISTER_WINDOW(Breakpoints);
REGISTER_WINDOW(CPUState);
REGISTER_WINDOW(PPUState);
REGISTER_WINDOW(Memory);

bool BreakpointInfo::Save(ostream& os, string& errmsg) const
{
    if(!address.Save(os, errmsg)) return false;
    WriteVarInt(os, (int)enabled);
    WriteVarInt(os, (int)has_bank);
    WriteVarInt(os, (int)(bool)condition);
    if(condition && !condition->Save(os, errmsg)) return false;
    WriteVarInt(os, (int)break_read);
    WriteVarInt(os, (int)break_write);
    WriteVarInt(os, (int)break_execute);
    if(!os.good()) {
        errmsg = "Error saving BreakpointInfo";
        return false;
    }
    return true;
}

bool BreakpointInfo::Load(istream& is, string& errmsg)
{
    if(!address.Load(is, errmsg)) return false;
    enabled = (bool)ReadVarInt<int>(is);
    has_bank = (bool)ReadVarInt<int>(is);
    if((bool)ReadVarInt<int>(is)) {
        condition = make_shared<Systems::NES::Expression>();
        if(!condition->Load(is, errmsg)) return false;
    }
    break_read = (bool)ReadVarInt<int>(is);
    break_write = (bool)ReadVarInt<int>(is);
    break_execute = (bool)ReadVarInt<int>(is);
    if(!is.good()) {
        errmsg = "Error loading BreakpointInfo";
        return false;
    }
    return true;
}

std::shared_ptr<SystemInstance> SystemInstance::CreateWindow()
{
    return make_shared<SystemInstance>();
}

SystemInstance::SystemInstance()
    : BaseWindow()
{
    system_id = next_system_id++;
    SetNav(false);

    SetShowMenuBar(true);
    SetIsDockSpace(true);

    SetHideOnClose(true);

    breakpoint_hit = make_shared<breakpoint_hit_t>();
    *child_window_added += std::bind(&SystemInstance::ChildWindowAdded, this, placeholders::_1);
    
    // allocate cpu_quick_breakpoints
    auto size = 0x10000 / (8 * sizeof(u32)); // one bit for 64KiB memory space
    cout << WindowPrefix() << "allocated " << dec << size << " bytes for CPU breakpoint cache" << endl;
    cpu_quick_breakpoints = new u32[size];
    memset(cpu_quick_breakpoints, 0, size);

    // allocate storage for framebuffers
    framebuffer = (u32*)new u8[sizeof(u32) * 256 * 256];

    // fill the framebuffer with fully transparent pixels (0), so the bottom 16 rows aren't visible
    memset(framebuffer, 0, 4 * 256 * 256);

    if(current_system = GetSystem()) {
        auto& mv = memory_view;

        ppu = make_shared<PPU>(
            [this](int high) {
                cpu->Nmi(high);
            },
            [this, &mv](u16 address)->u8 { // capturing the reference means the pointer can change after this initialization
                return memory_view->PeekPPU(address & 0x3FFF);
            },
            [this, &mv](u16 address)->u8 { 
                return memory_view->ReadPPU(address & 0x3FFF);
            },
            [this, &mv](u16 address, u8 value)->void {
                memory_view->WritePPU(address & 0x3FFF, value);
            }
        );

        apu_io = make_shared<APU_IO>();
        oam_dma_callback_connection = apu_io->oam_dma_callback->connect(std::bind(&SystemInstance::WriteOAMDMA, this, placeholders::_1));

        memory_view = current_system->CreateMemoryView(ppu->CreateMemoryView(), apu_io->CreateMemoryView());

        cpu = make_shared<CPU>(
            [this](u16 address, bool opcode_fetch)->u8 {
                [[unlikely]] if(cpu_quick_breakpoints[address >> 5] & (1 << (address & 0x1F))) {
                    CheckBreakpoints(address, opcode_fetch ? CheckBreakpointMode::EXECUTE : CheckBreakpointMode::READ);
                }
                return memory_view->Read(address);
            },
            [this](u16 address, u8 value)->void {
                [[unlikely]] if(cpu_quick_breakpoints[address >> 5] & (1 << (address & 0x1F))) {
                    CheckBreakpoints(address, CheckBreakpointMode::WRITE);
                }
                memory_view->Write(address, value);
            }
        );

        // start the emulation thread
        emulation_thread = make_shared<thread>(std::bind(&SystemInstance::EmulationThread, this));

        current_state = State::PAUSED;
    }

    // when this window becomes hidden, we should stop the emulation
    // TODO might be wise to exit the thread too, and then when the emulation starts again to recreate the thread
    *window_hidden += [this](shared_ptr<BaseWindow> const&) {
        if(current_state == State::RUNNING) {
            current_state = State::PAUSED;
            while(running) ;
        }
        UpdateTitle();
    };

    CreateStateVariableTable();

    Reset();
    UpdateTitle();
}

SystemInstance::~SystemInstance()
{
    exit_thread = true;
    if(emulation_thread) emulation_thread->join();

    delete [] (u8*)framebuffer;
    delete [] cpu_quick_breakpoints;
}

void SystemInstance::CreateStateVariableTable()
{
    auto& st = state_variable_table;

    st["a"]     = [&]() { return (s64)cpu->GetA(); };
    st["x"]     = [&]() { return (s64)cpu->GetX(); };
    st["y"]     = [&]() { return (s64)cpu->GetY(); };
    st["s"]     = [&]() { return (s64)cpu->GetS(); };
    st["p"]     = [&]() { return (s64)cpu->GetP(); };
    st["pc"]    = [&]() { return (s64)cpu->GetPC(); };
    st["istep"] = [&]() { return (s64)cpu->GetIStep(); };

    st["scanline"] = [&]() { return (s64)ppu->GetScanline(); };
    st["ppucycle"] = [&]() { return (s64)ppu->GetCycle(); };
    st["frame"]    = [&]() { return (s64)ppu->GetFrame(); };
}

void SystemInstance::RenderInstanceMenu()
{
    if(ImGui::BeginMenu("New Window")) {
        static char const * const window_types[] = {
            "Defines", "Regions", "Labels", "Listing", "Memory", 
            "Screen", "PPUState", "CPUState", "Watch", "Breakpoints", "Memory",
            "Enums"
        };

        for(int i = 0; i < IM_ARRAYSIZE(window_types); i++) {
            if(ImGui::MenuItem(window_types[i])) {
                CreateNewWindow(window_types[i]);
            }
        }

        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("States")) {
        if(ImGui::MenuItem("Save New State")) {
            auto new_state = CreateSaveState();
            save_states.push_back(new_state);
        }

        ImGui::Separator();

        int i = 0;
        for(auto& save_state : save_states) {
            ImGui::PushID(i++);
            if(ImGui::BeginMenu(save_state->name.c_str())) {
                auto const time = chrono::current_zone()->to_local(save_state->timestamp);

                string timestr = std::format("{:%c}", time);
                ImGui::Text("Last %s", timestr.c_str());
                ImGui::Separator();

                if(ImGui::MenuItem("Load Now")) {
                    LoadSaveState(save_state);
                }

                if(ImGui::MenuItem("Save Now")) {
                    auto new_state = CreateSaveState();
                    // free memory associated with the current state
                    delete [] save_state->data;
                    // copy over new_state values
                    save_state->timestamp = new_state->timestamp;
                    save_state->data_size = new_state->data_size;
                    save_state->data = new_state->data;
                }

                if(ImGui::MenuItem("Rename")) {
                    popups.edit_buffer = save_state->name;
                    popups.save_state_name.save_state = save_state;
                    popups.save_state_name.show = true;
                }

                if(ImGui::MenuItem("Delete")) {
                    popups.delete_state_name.save_state = save_state;
                    popups.delete_state_name.show = true;
                }

                ImGui::EndMenu();
            }
            ImGui::PopID();
        }

        ImGui::EndMenu();
    }

}

void SystemInstance::CreateDefaultWorkspace()
{
    Systems::NES::GlobalMemoryLocation where = {
        .address = 0xFD86,
        .is_chr = false,
        .prg_rom_bank = 3,
    };
    auto bpi = make_shared<BreakpointInfo>();
    bpi->address = where;
    bpi->has_bank = true;
    bpi->enabled = true;
    bpi->break_execute = true;
    SetBreakpoint(where, bpi);

    //CreateNewWindow("Regions"); // not a default window
    CreateNewWindow("Defines");
    CreateNewWindow("Labels");
    CreateNewWindow("Enums");
    CreateNewWindow("Listing");
    CreateNewWindow("Screen");
    CreateNewWindow("PPUState");
    CreateNewWindow("CPUState");
    CreateNewWindow("Watch");
    CreateNewWindow("Memory");
    CreateNewWindow("Breakpoints");
}

void SystemInstance::CreateNewWindow(string const& window_type)
{
    shared_ptr<BaseWindow> wnd;
    if(window_type == "Listing") {
        wnd = Listing::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_ROOT);
    } else if(window_type == "Defines") {
        wnd = Defines::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_LEFT);
    } else if(window_type == "Labels") {
        wnd = Labels::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_LEFT);
    } else if(window_type == "Regions") {
        wnd = MemoryRegions::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_LEFT);
    } else if(window_type == "Enums") {
        wnd = Enums::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_LEFT);
    } else if(window_type == "Screen") {
        wnd = Screen::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_RIGHTTOP);
    } else if(window_type == "CPUState") {
        wnd = CPUState::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_RIGHTBOTTOM);
    } else if(window_type == "PPUState") {
        wnd = PPUState::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_RIGHTBOTTOM);
    } else if(window_type == "Watch") {
        wnd = Watch::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_BOTTOMLEFT);
    } else if(window_type == "Breakpoints") {
        wnd = Breakpoints::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_BOTTOMRIGHT);
    } else if(window_type == "Memory") {
        wnd = Memory::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_BOTTOMLEFT);
    }

    AddChildWindow(wnd);
}

void SystemInstance::ChildWindowAdded(std::shared_ptr<BaseWindow> const& window)
{
    if(auto listing = dynamic_pointer_cast<Listing>(window)) {
        *window->window_activated += [this](shared_ptr<BaseWindow> const& _wnd) {
            most_recent_listing_window = _wnd;
        };
    }
}

void SystemInstance::ChildWindowRemoved(shared_ptr<BaseWindow> const& window)
{
    if(most_recent_listing_window == window) {
        most_recent_listing_window = nullptr;
    }
}

void SystemInstance::UpdateTitle()
{
    stringstream ss;
    ss << "NES_" << system_id;
    instance_name = ss.str();
    ss << " :: " << magic_enum::enum_name(current_state);
    system_title = ss.str();
    SetTitle(system_title.c_str());
}

void SystemInstance::Update(double deltaTime)
{
    UpdateTitle();

    if(step_instruction_done) {
        if(auto listing = dynamic_pointer_cast<Listing>(most_recent_listing_window)) {
            listing->GoToCurrentInstruction();
        }
        step_instruction_done = false;
    }

    // check for global keystrokes that should work in all windows
    bool is_current_instance = (GetSystemInstance().get() == this);
    if(is_current_instance) {
        if(ImGui::IsKeyPressed(ImGuiKey_F5)) {
            if(current_state == State::PAUSED) {
                current_state = State::RUNNING;
            }
        }

        if(ImGui::IsKeyPressed(ImGuiKey_F10)) {
            if(current_state == State::PAUSED) {
                current_state = State::STEP_INSTRUCTION;
            }
        }

        if(ImGui::IsKeyPressed(ImGuiKey_Escape) && ImGui::IsKeyPressed(ImGuiKey_LeftCtrl)) {
            if(current_state == State::RUNNING) {
                current_state = State::PAUSED;
            }
        }
    }

    if(thread_exited) {
        cout << "uh oh thread exited" << endl;
    }

    u64 cycle_count = cpu->GetCycleCount();
    auto current_time = chrono::steady_clock::now();
    u64 delta = cycle_count - last_cycle_count;
    double delta_time = (current_time - last_cycle_time) / 1.0s;
    if(delta_time >= 1.0) {
        cycles_per_sec = delta / delta_time;
        last_cycle_time = current_time;
        last_cycle_count = cycle_count;
    }
}

void SystemInstance::RenderMenuBar()
{
    if(current_state == State::PAUSED && ImGui::Button("Run")) {
        current_state = State::RUNNING;
    } else if(current_state == State::RUNNING && ImGui::Button("Stop")) {
        current_state = State::PAUSED;
    }

    auto last_state = current_state;
    if(current_state != State::PAUSED) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    ImGui::SameLine();
    if(ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
        if(ImGui::Button("Cycle")) {
            if(current_state == State::PAUSED) {
                current_state = State::STEP_CYCLE;
            }
        }
    } else {
        if(ImGui::Button("Step")) {
            if(current_state == State::PAUSED) {
                current_state = State::STEP_INSTRUCTION;
            }
        }
    }

    if(last_state != State::PAUSED) {
        ImGui::PopStyleVar();
        ImGui::PopItemFlag();
    }

    ImGui::SameLine();
    if(ImGui::Button("Reset")) {
        Reset();
    }

    ImGui::SameLine();
    ImGui::Text("%f Hz", cycles_per_sec);
}

void SystemInstance::CheckInput()
{
}

void SystemInstance::Render()
{
    if(popups.save_state_name.show) {
        if(auto ret = GetMainWindow()->InputNamePopup("Enter save state name", "Name", &popups.edit_buffer, true, true)) {
            if(ret > 0) {
                auto ss = popups.save_state_name.save_state;
                ss->name = popups.edit_buffer;
            }

            popups.save_state_name.show = false;
            popups.save_state_name.save_state = nullptr;
        }
    } else if(popups.delete_state_name.show) {
        stringstream ss;
        ss << "Deleting save state \"" << popups.delete_state_name.save_state->name << "\" cannot be undone. Continue?";
        if(auto ret = GetMainWindow()->OKCancelPopup("Delete save state", ss.str().c_str())) {
            if(ret > 0) {
                auto it = find_if(save_states.begin(), save_states.end(), [&](shared_ptr<SaveStateInfo> const& s)->bool {
                    if(s->data == popups.delete_state_name.save_state->data) return true;
                    return false;
                });

                if(it != save_states.end()) {
                    save_states.erase(it);
                }
            }

            popups.delete_state_name.show = false;
            popups.delete_state_name.save_state = nullptr;
        }
    }
}

void SystemInstance::Reset()
{
    auto saved_state = current_state;
    if(current_state == State::RUNNING) {
        current_state = State::PAUSED;
        while(running) ;
    }

    cpu->Reset();
    ppu->Reset();
    cpu_shift = 0;
    raster_line = framebuffer;
    raster_y = 0;
    oam_dma_enabled = false;

    current_state = saved_state;
}

void SystemInstance::GetCurrentInstructionAddress(GlobalMemoryLocation* out)
{
    out->is_chr = false;
    out->address = cpu->GetOpcodePC();
    out->prg_rom_bank = 0;
    if(!(out->address & 0x8000)) out->address = cpu->GetPC(); // for reset/times when opcode PC isn't set

    auto system_view = GetMemoryViewAs<Systems::NES::SystemView>();
    assert(system_view);

    if(out->address & 0x8000) {
        out->prg_rom_bank = system_view->GetCartridgeView()->GetRomBank(out->address);
    }

    int offset = 0;
    current_system->GetMemoryObject(*out, &offset);
    out->address -= offset;
}

bool SystemInstance::StepCPU()
{
    // TODO DMC DMA has priority over OAM DMA
    if(oam_dma_enabled && cpu->IsReadCycle()) { // CPU can only be halted on a read cycle
        // simulate a "halt" cycle
        if(!dma_halt_cycle_done) {
            dma_halt_cycle_done = true;
            return cpu->Step();
        }

        // technically we need a random alignment cycle, but we just emulate perfect alignment so our DMA will always 
        // take 513 cycles, never 514

        // and technically DMA is part of the CPU but alas...it's happening here
        if(!oam_dma_rw) { // read
            oam_dma_read_latch = memory_view->Read(oam_dma_source);
            oam_dma_rw ^= 1;
        } else {
            memory_view->Write(0x2004, oam_dma_read_latch);
            oam_dma_rw ^= 1;
            oam_dma_source += 1;
            if((oam_dma_source & 0xFF) == 0) oam_dma_enabled = 0;
        }

        cpu->DmaStep();
        return false;
    } else {
        return cpu->Step();
    }
}

void SystemInstance::StepPPU()
{
    bool hblank_new, vblank;
    int color = ppu->Step(hblank_new, vblank);
    if(vblank) { // on high vblank
        // reset frame buffer to new buffer, etc
        raster_line = framebuffer;
        raster_y = 0;
    } else if(hblank_new && hblank_new != hblank) { // on rising edge of hblank
        hblank = hblank_new;
        // move scanline down
        raster_line = &framebuffer[raster_y++ * 256];
        raster_x = 0;
    } else if(!hblank_new) {
        hblank = false;
        // display color
        raster_line[raster_x++] = (0xFF000000 | color);
    }
}

bool SystemInstance::SingleCycle()
{
    bool ret;

    // PPU clock is /4 master clock and CPU is /12 master clock, so it steps 3x as often
    switch(cpu_shift) {
    case 0:
        ret = StepCPU();
        StepPPU();
        StepPPU();
        break;
    case 1:
        StepPPU();
        ret = StepCPU();
        StepPPU();
        StepPPU();
        break;
    case 2:
        StepPPU();
        ret = StepCPU();
        StepPPU();
        StepPPU();
        StepPPU();
        break;
    }

    cpu_shift = (cpu_shift + 1) % 3;
    return ret;
}

void SystemInstance::EmulationThread()
{
    while(!exit_thread) {
        switch(current_state) {
        case State::INIT:
        case State::PAUSED:
            break;

        case State::STEP_CYCLE:
            running = true;
            SingleCycle();
            current_state = State::PAUSED;
            running = false;
            break;

        case State::STEP_INSTRUCTION:
            running = true;
            // execute cycles until opcode fetch happens
            while(current_state == State::STEP_INSTRUCTION && !SingleCycle()) ;

            // always go to paused after a step instruction
            current_state = State::PAUSED;

            // notify main thread that step instruction is done
            step_instruction_done = true;

            running = false;
            break;

        case State::RUNNING:
            running = true;
            while(!exit_thread && current_state == State::RUNNING) {
                SingleCycle();

                if(cpu->GetNextUC() < 0) {
                    // perform one more cycle just to print out invalid opcode message
                    cpu->Step();
                    current_state = State::CRASHED;
                    break;
                }
            }
            running = false;
            break;

        case State::CRASHED:
            running = false;
            break;

        default:
            assert(false);
            break;
        }
    }

    thread_exited = true;
}

void SystemInstance::WriteOAMDMA(u8 page)
{
    oam_dma_enabled = true;
    oam_dma_source = (page << 8);
    oam_dma_rw = 0;
    dma_halt_cycle_done = false;
}

bool SystemInstance::SetBreakpointCondition(std::shared_ptr<BreakpointInfo> const& breakpoint_info, 
        std::shared_ptr<BaseExpression> const& expression, std::string& errmsg)
{
    // before going to System::FixupExpression, set system state variables
    if(!FixupExpression(expression, errmsg)) return false;

    // fixup the expression allowing labels, defines, derefs, and enums
    FixupFlags fixup_flags = FIXUP_DEFINES | FIXUP_LABELS | FIXUP_DEREFS | FIXUP_ENUMS;
    if(!current_system->FixupExpression(expression, errmsg, fixup_flags)) return false;

    // now we need to Explore() and set the dereferences on the expression
    auto cb = [&](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
        auto deref = dynamic_pointer_cast<BaseExpressionNodes::DereferenceOp>(node);
        if(!deref) return true;

        auto deref_func = [&](s64 in, s64* out, string& errmsg)->bool {
            if(!memory_view) {
                errmsg = "Internal error";
                return false;
            }

            *out = memory_view->Peek(in);
            return true;
        };

        deref->SetDereferenceFunction(deref_func);
        return true;
    };

    if(!expression->Explore(cb, nullptr)) return false;

    // should be good to go now
    breakpoint_info->condition = expression;
    return true;
}


void SystemInstance::CheckBreakpoints(u16 address, CheckBreakpointMode mode)
{
    GlobalMemoryLocation where = {
        .address      = address,
        .is_chr       = 0,
        .prg_rom_bank = 0,
    };

    [[likely]] if(where.address & 0x8000) { // determine bank when in bankable space
        auto system_view = GetMemoryViewAs<Systems::NES::SystemView>();
        where.prg_rom_bank = system_view->GetCartridgeView()->GetRomBank(where.address);
    }

    auto check_bp = [&](shared_ptr<BreakpointInfo> const& bp)->bool {
        auto break_execute = bp->break_execute && (mode == CheckBreakpointMode::EXECUTE);
        auto break_read    = bp->break_read    && (mode == CheckBreakpointMode::READ);
        auto break_write   = bp->break_write   && (mode == CheckBreakpointMode::WRITE);
        if(bp->enabled && (break_read || break_write || break_execute)) {
            // check condition
            s64 result;
            string errmsg;
            if(!bp->condition || (bp->condition->Evaluate(&result, errmsg) && (result != 0))) {
                current_state = State::PAUSED;
                // update the bank at which the breakpoint occurred -- either it'll be
                // re-set to the same value or has_bank is false and needs to be set anyway
                bp->address.prg_rom_bank = where.prg_rom_bank;
                breakpoint_hit->emit(bp);
                return true;
            }
        }
        return false;
    };

    // check both bank-specific and non-bank specific addresses
    auto bplist = GetBreakpointsAt(where);
    [[likely]] for(auto& bpiter : bplist) {
        if(check_bp(bpiter)) return;
    }

    bplist = GetBreakpointsAt(address);
    [[unlikely]] for(auto& bpiter : bplist) {
        if(check_bp(bpiter)) return;
    }
}

bool SystemInstance::FixupExpression(shared_ptr<BaseExpression> const& expression, string& errmsg)
{
    ExploreData ed = {
        .errmsg = errmsg,
    };

    // explore the expression and convert Names to SystemInstanceState nodes
    return expression->Explore(quick_bind(&SystemInstance::ExploreCallback, this), (void*)&ed);
}

bool SystemInstance::ExploreCallback(shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void* userdata)
{
    ExploreData* ed = (ExploreData*)userdata;
    auto nc = GetSystem()->GetNodeCreator();

    if(auto name = dynamic_pointer_cast<BaseExpressionNodes::Name>(node)) {
        auto state_variable = name->GetString();
        auto state_variable_lower = strlower(state_variable);

        // if the state_variable is valid, create a SystemInstanceState
        if(state_variable_table.contains(state_variable_lower)) {
            cout << "found " << state_variable << endl;

            // replace 'node' with a new type 
            node = nc->CreateSystemInstanceState(state_variable);
        }
    }

    // the above Name to SystemInstanceState could have happened, but also some already existing SystemInstanceState 
    // need to have the getter function set
    if(auto sis = dynamic_pointer_cast<Systems::NES::ExpressionNodes::SystemInstanceState>(node)) {
        auto state_variable_lower = strlower(sis->GetString());
        sis->SetGetStateFunction(state_variable_table[state_variable_lower]);
    }

    return true;
}

std::shared_ptr<SaveStateInfo> SystemInstance::CreateSaveState()
{
    stringstream oss;
    string errmsg;

    // system has to be paused to prevent modification while executing
    auto last_state = current_state;
    if(current_state != State::PAUSED) {
        current_state = State::PAUSED;
        while(running) ;
    }

    WriteVarInt(oss, 1); // reserved, must be 1

    // save CPU
    if(!cpu->Save(oss, errmsg)) return nullptr;

    // save PPU
    if(!ppu->Save(oss, errmsg)) return nullptr;

    // save APU_IO
    if(!apu_io->Save(oss, errmsg)) return nullptr;

    // save memory_view (i.e., all VRAM, RAM, cart state, etc)
    if(!memory_view->Save(oss, errmsg)) return nullptr;

    // save DMA state
    WriteVarInt(oss, (int)oam_dma_enabled);
    WriteVarInt(oss, oam_dma_source);
    WriteVarInt(oss, oam_dma_rw);
    WriteVarInt(oss, oam_dma_read_latch);
    WriteVarInt(oss, (int)dma_halt_cycle_done);

    // save a copy of the frame buffer so we can display it when state loads without running
    oss.write((char*)framebuffer, sizeof(u32) * 256 * 256);

    // and the raster positions
    WriteVarInt(oss, (int)hblank);
    WriteVarInt(oss, raster_x);
    WriteVarInt(oss, raster_y);

    // create a new SaveStateInfo
    auto new_state = make_shared<SaveStateInfo>();
    new_state->timestamp = chrono::system_clock::now();

    int x = save_states.size(); // set a default name
    stringstream ss;
    ss << "Save state " << x;
    new_state->name = ss.str();

    // get the data from stream
    auto buf = oss.rdbuf();
    new_state->data_size = oss.tellp();
    new_state->data = new u8[new_state->data_size];
    buf->sgetn((char*)new_state->data, new_state->data_size);

    // done, return current_state to its original value
    current_state = last_state;

    cout << WindowPrefix() << "save state \"" << new_state->name << "\" is " << dec << new_state->data_size << " bytes" << endl;
    return new_state;
}

bool SystemInstance::LoadSaveState(std::shared_ptr<SaveStateInfo> const& save_state)
{
    string errmsg;

    // create an istringstream with save_state's buffer
    auto buf = save_state->GetMembuf();
    istream is(&buf);

    // system has to be paused to prevent modification while executing
    auto last_state = current_state;
    if(current_state != State::PAUSED) {
        current_state = State::PAUSED;
        while(running) ;
    }

    int r = ReadVarInt<int>(is); // reserved, must be 1
    assert(r == 1);

    // load CPU
    if(!cpu->Load(is, errmsg)) return false;

    // load PPU
    if(!ppu->Load(is, errmsg)) return false;

    // load APU_IO
    if(!apu_io->Load(is, errmsg)) return false;

    // load memory_view
    if(!memory_view->Load(is, errmsg)) return false;

    // load DMA state
    oam_dma_enabled     = (bool)ReadVarInt<int>(is);
    oam_dma_source      = ReadVarInt<u16>(is);
    oam_dma_rw          = ReadVarInt<u8>(is);
    oam_dma_read_latch  = ReadVarInt<u8>(is);
    dma_halt_cycle_done = (bool)ReadVarInt<int>(is);

    // load framebuffer copy
    is.read((char*)framebuffer, sizeof(u32) * 256 * 256);

    // and the raster positions
    hblank = (bool)ReadVarInt<int>(is);
    raster_x = ReadVarInt<int>(is);
    raster_y = ReadVarInt<int>(is);

    // fixup raster_line to point to the correct row
    // raster_y == 0 means we're in vblank and will set render_line later
    if(raster_y > 0) raster_line = &framebuffer[(raster_y - 1) * 256];

    errmsg = "Error loading state";
    return is.good();
}

bool SystemInstance::SaveWindow(std::ostream& os, std::string& errmsg)
{
    auto last_state = current_state;
    if(current_state != State::PAUSED) {
        current_state = State::PAUSED;
        while(running) ; 
    }

    WriteVarInt(os, next_system_id); // every instance of SystemInstance will save and write the same value, but whatever...
    WriteVarInt(os, system_id);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // serialize breakpoints
    WriteVarInt(os, breakpoints.size());
    for(auto& bppair: breakpoints) {
        if(auto where_ptr = get_if<GlobalMemoryLocation>(&bppair.first)) {
            GlobalMemoryLocation const& where = *where_ptr;
            WriteVarInt(os, 0); // key type
            if(!where.Save(os, errmsg)) return false;
        } else if(auto address_ptr = get_if<u16>(&bppair.first)) {
            u16 const& address = *address_ptr;
            WriteVarInt(os, 1); // key type
            WriteVarInt(os, address);
        }
        
        // number of breakpoints at this key
        auto& bplist = bppair.second;
        WriteVarInt(os, bplist.size());
        for(auto& bpi: bplist) {
            if(!bpi->Save(os, errmsg)) return false;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // serialize save states
    WriteVarInt(os, save_states.size());
    for(auto& save_state : save_states) {
        if(!save_state->Save(os, errmsg)) return false;
    }

    // serialize the /current/ state as well
    auto current_save_state = CreateSaveState();
    if(!current_save_state->Save(os, errmsg)) return false;

    current_state = last_state;
    return true;
}

bool SystemInstance::LoadWindow(std::istream& is, std::string& errmsg)
{
    assert(current_state == State::PAUSED); // should be the default initialization state

    next_system_id = ReadVarInt<int>(is);
    system_id = ReadVarInt<int>(is);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // deserialize breakpoints
    int breakpoints_size = ReadVarInt<int>(is);
    for(int i = 0; i < breakpoints_size; i++) {
        int key_type = ReadVarInt<int>(is);
        breakpoint_key_t key;
        if(key_type == 0) { // read GlobalMemoryLocation
            GlobalMemoryLocation where;
            if(!where.Load(is, errmsg)) return false;
            key = where;
        } else if(key_type == 1) { // read u16
            key = ReadVarInt<u16>(is);
        }

        int num_breakpoints = ReadVarInt<int>(is);
        for(int j = 0; j < num_breakpoints; j++) {
            auto bpi = make_shared<BreakpointInfo>();
            if(!bpi->Load(is, errmsg)) return false;

            // condition expressions need SystemInstanceState updated
            if(bpi->condition && !FixupExpression(bpi->condition, errmsg)) return false;

            SetBreakpoint(key, bpi);
        }
    }
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // deserialize save states
    if(GetCurrentProject()->GetSaveFileVersion() >= FILE_VERSION_SAVE_STATES) {
        int save_states_size = ReadVarInt<int>(is);
        for(int i = 0; i < save_states_size; i++) {
            auto save_state = make_shared<SaveStateInfo>();
            if(!save_state->Load(is, errmsg)) return false;
            save_states.push_back(save_state);
        }

        // load the current state
        auto current_save_state = make_shared<SaveStateInfo>();
        if(!current_save_state->Load(is, errmsg)) return false;

        // and immediately load it
        LoadSaveState(current_save_state);
    }

    return true;
}

std::shared_ptr<Screen> Screen::CreateWindow()
{
    return make_shared<Screen>();
}

Screen::Screen()
    : BaseWindow()
{
    SetNav(false);
    SetNoScrollbar(true);
    SetTitle("Screen");

}

Screen::~Screen()
{
    GLuint gl_texture = (GLuint)(intptr_t)framebuffer_texture;
    glDeleteTextures(1, &gl_texture);
}

void Screen::CheckInput()
{
    // only if Screen window is active, check the keyboard inputs
    // TODO joystick input might be better off in SystemInstance::Update(), since
    // we will probably want to accept input when Screen is not in focus
    if(auto apu_io = GetMySystemInstance()->GetAPUIO()) {
        apu_io->SetJoy1Pressed(NES_BUTTON_UP    , ImGui::IsKeyDown(ImGuiKey_W));
        apu_io->SetJoy1Pressed(NES_BUTTON_DOWN  , ImGui::IsKeyDown(ImGuiKey_S));
        apu_io->SetJoy1Pressed(NES_BUTTON_LEFT  , ImGui::IsKeyDown(ImGuiKey_A));
        apu_io->SetJoy1Pressed(NES_BUTTON_RIGHT , ImGui::IsKeyDown(ImGuiKey_D));
        apu_io->SetJoy1Pressed(NES_BUTTON_SELECT, ImGui::IsKeyDown(ImGuiKey_Tab));
        apu_io->SetJoy1Pressed(NES_BUTTON_START , ImGui::IsKeyDown(ImGuiKey_Enter));
        apu_io->SetJoy1Pressed(NES_BUTTON_B     , ImGui::IsKeyDown(ImGuiKey_Period));
        apu_io->SetJoy1Pressed(NES_BUTTON_A     , ImGui::IsKeyDown(ImGuiKey_Slash));
    }
}

void Screen::Update(double deltaTime)
{
}

void Screen::PreRender()
{
    // won't really be necessary if the window starts docked
    ImGui::SetNextWindowSize(ImVec2(324, 324), ImGuiCond_Appearing);
}

void Screen::Render()
{
    if(!valid_texture) {
        GLuint gl_texture;
        glGenTextures(1, &gl_texture);
        glBindTexture(GL_TEXTURE_2D, gl_texture);
        // OpenGL requires at least one glTexImage2D to setup the texture
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        framebuffer_texture = (void*)(intptr_t)gl_texture;
        glBindTexture(GL_TEXTURE_2D, 0);
        valid_texture = true;
    }

    if(auto framebuffer = GetMySystemInstance()->GetFramebuffer()) {
        GLuint gl_texture = (GLuint)(intptr_t)framebuffer_texture;
        glBindTexture(GL_TEXTURE_2D, gl_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, framebuffer);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    auto size = ImGui::GetWindowSize();
    float sz = min(size.x, size.y);

    //TODO could do some toggles like keep aspect ratio, scale to window size, etc

    ImGui::Image(framebuffer_texture, ImVec2(sz, sz));
}

std::shared_ptr<CPUState> CPUState::CreateWindow()
{
    return make_shared<CPUState>();
}

CPUState::CPUState()
    : BaseWindow()
{
    SetTitle("CPU");
}

CPUState::~CPUState()
{
}

void CPUState::CheckInput()
{
}

void CPUState::Update(double deltaTime)
{
}

void CPUState::Render()
{
    auto disassembler = GetSystem()->GetDisassembler();
    if(!disassembler) return;

    auto si = GetMySystemInstance();
    auto cpu = si->GetCPU();
    if(!cpu) return;

    auto memory_view = si->GetMemoryView();
    if(!memory_view) return;

    u64 next_uc = cpu->GetNextUC();
    if(next_uc == (u64)-1) {
        ImGui::Text("$%04X: Invalid opcode $%02X", cpu->GetOpcodePC()-1, cpu->GetOpcode());
    } else {
        string inst = disassembler->GetInstruction(cpu->GetOpcode());
        auto pc = cpu->GetOpcodePC();
        u8 operands[] = { memory_view->Read(pc+1), memory_view->Read(pc+2) };
        string operand = disassembler->FormatOperand(cpu->GetOpcode(), operands);
        if(ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
            ImGui::Text("$%04X: %s %s (istep %d, uc=0x%X)", pc, inst.c_str(), operand.c_str(), cpu->GetIStep(), next_uc);
        } else {
            ImGui::Text("$%04X: %s %s", pc, inst.c_str(), operand.c_str());
        }
    }

    ImGui::Separator();

    ImGui::Text("PC:$%04X", cpu->GetPC()); ImGui::SameLine();
    ImGui::Text("S:$%04X", cpu->GetS()); ImGui::SameLine();
    ImGui::Text("A:$%02X", cpu->GetA()); ImGui::SameLine();
    ImGui::Text("X:$%02X", cpu->GetX()); ImGui::SameLine();
    ImGui::Text("Y:$%02X", cpu->GetY());

    u8 p = cpu->GetP();
    char flags[] = "P:nv-bdizc";
    if(p & CPU_FLAG_N) flags[2] = 'N';
    if(p & CPU_FLAG_V) flags[3] = 'V';
    if(p & CPU_FLAG_B) flags[5] = 'B';
    if(p & CPU_FLAG_D) flags[6] = 'D';
    if(p & CPU_FLAG_I) flags[7] = 'I';
    if(p & CPU_FLAG_Z) flags[8] = 'Z';
    if(p & CPU_FLAG_C) flags[9] = 'C';
    ImGui::Text("%s", flags);
}

std::shared_ptr<PPUState> PPUState::CreateWindow()
{
    return make_shared<PPUState>();
}

PPUState::PPUState()
    : BaseWindow()
{
    SetTitle("PPU");

    // allocate storage for the nametable rendering
    nametable_framebuffer = new u32[512 * 512];
    memset(nametable_framebuffer, 0, sizeof(u32) * 512 * 512);

    // allocate storage for sprites rendering
    // clearing the image to white makes our dividing lines white and we don't have to draw lines
    sprites_framebuffer = new u32[128 * 128];
    memset(sprites_framebuffer, 0xFF, sizeof(u32) * 128 * 128);

    // allocate storage for two pattern tables with no dividing line
    for(int i = 0; i < 2; i++) {
        pattern_framebuffer[i] = new u32[128 * 128];
        memset(pattern_framebuffer[i], 0x00, sizeof(u32) * 128 * 128);
    }
}

PPUState::~PPUState()
{
    GLuint gl_texture = (GLuint)(intptr_t)nametable_texture;
    glDeleteTextures(1, &gl_texture);

    gl_texture = (GLuint)(intptr_t)sprites_texture;
    glDeleteTextures(1, &gl_texture);

    for(int i = 0; i < 2; i++) {
        gl_texture = (GLuint)(intptr_t)pattern_texture[i];
        glDeleteTextures(1, &gl_texture);
    }

    delete [] nametable_framebuffer;
    delete [] sprites_framebuffer;
    delete [] pattern_framebuffer[0];
    delete [] pattern_framebuffer[1];
}

void PPUState::CheckInput()
{
}

void PPUState::Update(double deltaTime)
{
    if(!valid_texture) {
        // generate the nametable GL texture
        GLuint gl_texture[2];

        glGenTextures(1, &gl_texture[0]);
        glBindTexture(GL_TEXTURE_2D, gl_texture[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        nametable_texture = (void*)(intptr_t)gl_texture[0];

        // make an 8x8 view of the 64 sprites with a dividing line between all
        // sprites are 8x8, so there are 8*8+7=71 pixels in each direction.
        // next power of two is 128, so allocate a 128x128 texture
        glGenTextures(1, &gl_texture[0]);
        glBindTexture(GL_TEXTURE_2D, gl_texture[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        sprites_texture = (void*)(intptr_t)gl_texture[0];

        // two 16 tile by 16 tile pattern textures, each 8x8 pixels
        glGenTextures(2, &gl_texture[0]);
        for(int i = 0; i < 2; i++) {
            glBindTexture(GL_TEXTURE_2D, gl_texture[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            pattern_texture[i] = (void*)(intptr_t)gl_texture[i];
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        valid_texture = true;
    }

    if(display_mode == 1) {
        UpdateNametableTexture();
    } else if(display_mode == 3) {
        UpdateSpriteTexture();
    } else if(display_mode == 4) {
        UpdatePatternTextures();
    }
}

void PPUState::PreRender()
{
    // show horizontal scroll only on the pattern table view
    SetHorizontalScroll(display_mode == 4);
}

void PPUState::Render()
{
    auto si = GetMySystemInstance();
    auto ppu = si->GetPPU();
    if(!ppu) return;

    auto memory_view = si->GetMemoryView();
    if(!memory_view) return;

    ImGui::Combo("View", &display_mode, "Registers\0Nametables\0Palettes\0Sprites\0Pattern Tables\0\0");
    ImGui::Separator();

    switch(display_mode) {
    case 0:
        RenderRegisters(ppu);
        break;

    case 1:
        RenderNametables(ppu);
        break;

    case 2:
        RenderPalettes(ppu);
        break;

    case 3:
        RenderSprites(ppu);
        break;

    case 4:
        RenderPatternTables(ppu);
        break;
    }
}

void PPUState::RenderRegisters(std::shared_ptr<PPU> const& ppu)
{
    bool open;
    u8 v;
    u16 addr;

    ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoBordersInBodyUntilResize
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchSame;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(-1, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(-1, 0));

    // We use nested tables so that each row can have its own layout. This will be useful when we can render
    // things like plate comments, labels, etc
    if(ImGui::BeginTable("ppustats_registers_table", 3, table_flags)) {
        ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);

        // Begin a new row and next column
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Frame Index");
        ImGui::TableNextColumn();
        ImGui::Text("%d", ppu->GetFrame());

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Scanline");
        ImGui::TableNextColumn();
        ImGui::Text("%d", ppu->GetScanline());

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Cycle");
        ImGui::TableNextColumn();
        ImGui::Text("%d", ppu->GetCycle());

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Scroll X");
        ImGui::TableNextColumn();
        ImGui::Text("%d", ppu->GetScrollX());

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Scroll Y");
        ImGui::TableNextColumn();
        ImGui::Text("%d", ppu->GetScrollY());

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        open = ImGui::TreeNodeEx("VRAM bus address", ImGuiTreeNodeFlags_SpanFullWidth);
        addr = ppu->GetVramAddress();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("$%04X", addr);
        ImGui::TableNextColumn();
        ImGui::Text("Value currently on VRAM address bus");
        if(open) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Intermediate VRAM address");
            addr = ppu->GetVramAddressT();
            ImGui::TableNextColumn();
            ImGui::Text("$%04X", addr);
            ImGui::TableNextColumn();
            ImGui::Text("Loopy T");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Final VRAM address");
            addr = ppu->GetVramAddressV();
            ImGui::TableNextColumn();
            ImGui::Text("$%04X", addr);
            ImGui::TableNextColumn();
            ImGui::Text("Loopy V");

            ImGui::TreePop();
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        open = ImGui::TreeNodeEx("[PPUCONT] $2000", ImGuiTreeNodeFlags_SpanFullWidth);
        v = ppu->GetPPUCONT();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("$%02X", v);

        if(open) {
            //u8 base_nametable_address           : 2;
            //u8 vram_increment                   : 1;
            //u8 sprite_pattern_table_address     : 1;
            //u8 background_pattern_table_address : 1;
            //u8 sprite_size                      : 1;
            //u8 _master_slave                    : 1; // unused
            //u8 enable_nmi                       : 1;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[ NT] $2000.01");
            ImGui::TableNextColumn();
            ImGui::Text("$%x", v & 0x03);
            ImGui::TableNextColumn();
            ImGui::Text("Nametable @ $%04X", 0x2000 | ((v & 0x03) << 10));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[ VI] $2000.2");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x04) >> 2);
            ImGui::TableNextColumn();
            ImGui::Text("VRAM increment %d", (v & 0x04) ? 32 : 1);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[SPT] $2000.3");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x08) >> 3);
            ImGui::TableNextColumn();
            ImGui::Text("Sprite tiles @ $%04X", (v & 0x08) ? 0x1000 : 0);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[BGT] $2000.4");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x10) >> 4);
            ImGui::TableNextColumn();
            ImGui::Text("BG tiles @ $%04X", (v & 0x10) ? 0x1000 : 0);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[SSZ] $2000.5");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x20) >> 5);
            ImGui::TableNextColumn();
            ImGui::Text("Sprite size 8x%d", (v & 0x20) ? 16 : 8);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[NMI] $2000.7");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x80) >> 7);
            ImGui::TableNextColumn();
            ImGui::Text("NMI %s", (v & 0x80) ? "enabled" : "disabled");

            ImGui::TreePop();
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        open = ImGui::TreeNodeEx("[PPUMASK] $2001", ImGuiTreeNodeFlags_SpanFullWidth);
        v = ppu->GetPPUMASK();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("$%02X", v);
        if(open) {
            //u8 greyscale             : 1;
            //u8 show_background_left8 : 1;
            //u8 show_sprites_left8    : 1;
            //u8 show_background       : 1;
            //u8 show_sprites          : 1;
            //u8 emphasize_red         : 1;
            //u8 emphasize_green       : 1;
            //u8 emphasize_blue        : 1;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[GRY] $2001.00");
            ImGui::TableNextColumn();
            ImGui::Text("%d", v & 0x01);
            ImGui::TableNextColumn();
            ImGui::Text((v & 0x01) ? "Greyscale" : "Not greyscale");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[BL8] $2001.01");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x02) >> 1);
            ImGui::TableNextColumn();
            ImGui::Text((v & 0x02) ? "Show left 8 BG pixels" : "Don't show left 8 BG pixels");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[SL8] $2001.02");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x04) >> 2);
            ImGui::TableNextColumn();
            ImGui::Text((v & 0x04) ? "Show left 8 sprite pixels" : "Don't show left 8 sprite pixels");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[BGE] $2001.03");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x08) >> 3);
            ImGui::TableNextColumn();
            ImGui::Text((v & 0x08) ? "Show BG" : "Don't show BG");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[BGE] $2001.04");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x10) >> 4);
            ImGui::TableNextColumn();
            ImGui::Text((v & 0x10) ? "Show Sprites" : "Don't show Sprites");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[BGE] $2001.05");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x20) >> 5);
            ImGui::TableNextColumn();
            ImGui::Text((v & 0x20) ? "Emphasize RED" : "Normal RED");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[BGE] $2001.06");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x40) >> 6);
            ImGui::TableNextColumn();
            ImGui::Text((v & 0x40) ? "Emphasize GREEN" : "Normal GREEN");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[BGE] $2001.07");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x80) >> 7);
            ImGui::TableNextColumn();
            ImGui::Text((v & 0x80) ? "Emphasize BLUE" : "Normal BLUE");

            ImGui::TreePop();
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        open = ImGui::TreeNodeEx("[PPUSTAT] $2002", ImGuiTreeNodeFlags_SpanFullWidth);
        v = ppu->GetPPUSTAT();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("$%02X", v);
        if(open) {
            //u8 unused0         : 5;
            //u8 sprite_overflow : 1;
            //u8 sprite0_hit     : 1;
            //u8 vblank          : 1;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[SOV] $2002.05");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x20) >> 5);
            ImGui::TableNextColumn();
            ImGui::Text((v & 0x20) ? "Sprite overflow" : "No sprite overflow");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[S0H] $2002.06");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x40) >> 6);
            ImGui::TableNextColumn();
            ImGui::Text((v & 0x20) ? "Sprite 0 hit" : "No sprite 0 hit");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("[VBL] $2002.07");
            ImGui::TableNextColumn();
            ImGui::Text("%d", (v & 0x80) >> 7);
            ImGui::TableNextColumn();
            ImGui::Text((v & 0x80) ? "In VBlank" : "Not in VBlank");

            ImGui::TreePop();
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar(2);
}

void PPUState::RenderNametables(std::shared_ptr<PPU> const& ppu)
{
    ImGuiFlagButton(&show_scroll_window, "S", "Show Scroll Window");
    ImGui::Separator();

    ImVec2 size = ImGui::GetWindowSize();
    float sz = size.x < size.y ? size.x : size.y;
    sz *= 0.9;
    ImGui::Image(nametable_texture, ImVec2(sz, sz));//480/512 * sz));
}

void PPUState::RenderPalettes(std::shared_ptr<PPU> const& ppu)
{
    u8 bg_palette[0x10];
    ppu->CopyPaletteRAM(bg_palette, false);

    u8 obj_palette[0x10];
    ppu->CopyPaletteRAM(obj_palette, true);

    /////////////////////////////////////////////////////////////////////////////////////////////
    auto size = ImGui::GetWindowSize();
    size.x *= 0.667;
    if(ImGui::BeginChild("left", size)) {
        ImGuiTableFlags table_flags = ImGuiTableFlags_NoBordersInBodyUntilResize
            | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchSame;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(-1, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(-1, 0));

        // We use nested tables so that each row can have its own layout. This will be useful when we can render
        // things like plate comments, labels, etc
        if(ImGui::BeginTable("palette_table", 4, table_flags)) {
            ImGui::TableSetupColumn("0", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
            ImGui::TableSetupColumn("1", ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);
            ImGui::TableSetupColumn("2", ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);
            ImGui::TableSetupColumn("3", ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);
            ImGui::TableHeadersRow();

            for(int i = 0; i < 8; i++) {
                ImGui::TableNextRow();
                for(int j = 0; j < 4; j++) {
                    u8 color = (i < 4) ? bg_palette[i * 4 + j] : obj_palette[(i - 4) * 4 + j];
                    ImU32 im_color = 0xFF000000 | Systems::NES::rgb_palette_map[color];

                    ImGui::TableNextColumn();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, im_color);

                    ImGui::Selectable(" ", false, 0);
                    if(ImGui::IsItemHovered()) {
                        // update display for hovered color
                        hovered_palette_index = (i * 4) + j;
                    }
                }
            }

            ImGui::EndTable();
        }
        ImGui::PopStyleVar(2);

    }
    ImGui::EndChild();

    /////////////////////////////////////////////////////////////////////////////////////////////
    ImGui::SameLine();
    if(ImGui::BeginChild("right", size, true)) {
        u8 color = (hovered_palette_index < 0x10) ? bg_palette[hovered_palette_index] : obj_palette[hovered_palette_index - 0x10];
        int rgb_color = Systems::NES::rgb_palette_map[color];
        ImGui::Text("[$3F%02X]=$%02X", hovered_palette_index, color);
        ImGui::Text("");
        int red = (rgb_color & 0xFF);
        ImGui::Text("R: %d (0x%02X)", red, red);
        int green = (rgb_color & 0xFF00) >> 8;
        ImGui::Text("G: %d (0x%02X)", green, green);
        int blue = (rgb_color & 0xFF0000) >> 16;
        ImGui::Text("B: %d (0x%02X)", blue, blue);

        char buf[12];
        sprintf(buf, "#%02X%02X%02X", red, green, blue);
        ImGui::InputText("HTML", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::EndChild();
}

void PPUState::RenderSprites(std::shared_ptr<PPU> const& ppu)
{
    ImVec2 size = ImGui::GetWindowSize();
    float sz = size.x < size.y ? size.x : size.y;
    sz *= 0.9;
    // sprites are 8x8 with 1 pixel between each = 71x71 pixels, but texture is 128x128
    ImGui::Image(sprites_texture, ImVec2(sz, sz), ImVec2(0, 0), ImVec2(71.0/128.0, 71.0/128.0));

    if(ImGui::IsItemHovered()) {
        auto loc   = ImGui::GetItemRectMin();
        auto mouse = ImGui::GetMousePos();
        // convert mouse coordinate relative to image location into pixels from 0..71, and dividing by 9 gives us our x/y index
        int dx     = round(71.0 * (mouse.x - loc.x) / sz);
        int dy     = round(71.0 * (mouse.y - loc.y) / sz);
        int si     = (dy / 9) * 8 + (dx / 9);

        // grab the OAM data
        u8* oam_ptr = &oam_copy[si << 2];
        u8  y       = *oam_ptr++;
        u8  tile    = *oam_ptr++;
        u8  attr    = *oam_ptr++;
        u8  x       = *oam_ptr++;

        stringstream ss;
        ss << "Sprite #" << si << "\n\n";
        ss << "Tile: $" << hex << uppercase << (int)tile << " (" << dec << (int)tile << ")\n";
        ss << "Pos: (" << (int)x << "," << (int)y << ")\n";
        ss << "Palette: " << (int)(attr & 0x03) << "\n";
        ss << "Priority: " << ((attr & 0x20) ? "Back" : "Front") << "\n";
        ss << "XFlip: " << ((attr & 0x40) ? "1" : "0") << "\n";
        ss << "YFlip: " << ((attr & 0x80) ? "1" : "0") << "\n";

        ImGui::SetTooltip(ss.str().c_str());
    }
}

void PPUState::RenderPatternTables(std::shared_ptr<PPU> const& ppu)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "Palette %d", palette_index);

    if(ImGui::BeginCombo("Palette", buf)) {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(-1, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(-1, 0));

        ImGuiTableFlags table_flags = ImGuiTableFlags_NoBordersInBodyUntilResize
            | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchSame;

        // We use nested tables so that each row can have its own layout. This will be useful when we can render
        // things like plate comments, labels, etc
        if(ImGui::BeginTable("palette_table", 5, table_flags)) {
            ImGui::TableSetupColumn("##0", ImGuiTableColumnFlags_WidthStretch, 0.0f);
            ImGui::TableSetupColumn("##1", ImGuiTableColumnFlags_WidthStretch, 0.0f);
            ImGui::TableSetupColumn("##2", ImGuiTableColumnFlags_WidthStretch, 0.0f);
            ImGui::TableSetupColumn("##3", ImGuiTableColumnFlags_WidthStretch, 0.0f);
            ImGui::TableSetupColumn("##Text", ImGuiTableColumnFlags_WidthFixed, 0.0f);

            for(int i = 0; i < 8; i++) {
                ImGui::TableNextRow();
                u8* palette = &palette_copy[i << 2];
                for(int j = 0; j < 4; j++) {
                    ImU32 im_color = 0xFF000000 | Systems::NES::rgb_palette_map[palette[j]];

                    ImGui::TableNextColumn();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, im_color);

                    ImGui::PushID(i * 4 + j);
                    if(ImGui::Selectable(" ", false, ImGuiSelectableFlags_SpanAllColumns)) {
                        palette_index = i;
                    }
                    ImGui::PopID();
                }

                ImGui::TableNextColumn();
                ImGui::Text("%s %d", (i < 4) ? "BG" : "SPR", i % 4);
            }

            ImGui::EndTable();
        }

        ImGui::PopStyleVar(2);
        ImGui::EndCombo();
    }

    ImVec2 size = ImGui::GetWindowSize();
    float sz = size.x < size.y ? size.x : size.y;
    sz *= 0.9;

    for(int i = 0; i < 2; i++) {
        ImGui::BeginGroup();

        ImGui::Text("Table $%04X:", i << 12);
        ImGui::Image(pattern_texture[i], ImVec2(sz, sz));
        if(ImGui::IsItemHovered()) {
        }

        ImGui::EndGroup();

        // display horizontally if window is wider than it is tall
        if(i == 0 && size.x > size.y) ImGui::SameLine();
    }
}

void PPUState::UpdateNametableTexture()
{
    int cx = 0;
    int cy = 0;
    int sz = 5;

    auto render_screen = [](u32* fb, u8* nametable, u8* bg_patterns, u8* palette_ram, int fx, int fy) {
        u8* attrtable = &nametable[0x3C0];
        for(int ty = 0; ty < 30; ty++) {
            for(int tx = 0; tx < 32; tx++) {
                u8 tile = *nametable++;

                u8 attr = attrtable[8 * (ty / 4) + tx / 4];
                if((ty & 0x02)) attr >>= 4;
                if((tx & 0x02)) attr >>= 2;
                attr &= 0x03;

                // render 8x8
                for(int y = 0; y < 8; y++) {
                    u8 row0 = bg_patterns[(u16)(tile << 4) + y + 0x00];
                    u8 row1 = bg_patterns[(u16)(tile << 4) + y + 0x08];

                    int cy = fy + ty * 8 + y;
                    for(int x = 0; x < 8; x++) {
                        int b0 = (row0 & 0x80) >> 7; row0 <<= 1;
                        int b1 = (row1 & 0x80) >> 7; row1 <<= 1;
                        int pal = (attr << 2) | (b1 << 1) | b0;

                        // use BG color for color 0
                        if(b0 == 0 && b1 == 0) pal = 0;

                        int color = palette_ram[pal & 0x0F] & 0x3F;
                        int cx = fx + tx * 8 + x;
                        fb[cy * 512 + cx] = 0xFF000000 | Systems::NES::rgb_palette_map[color];
                    }
                }
            }
        }
    };

    auto si = GetMySystemInstance();
    auto ppu = si->GetPPU();

    if(auto system_view = dynamic_pointer_cast<Systems::NES::SystemView>(si->GetMemoryView())) {
        u8 vram[0x800];
        system_view->CopyVRAM(vram);

        u8 bg_patterns[0x1000];
        auto cartridge_view = system_view->GetCartridgeView();
        u16 bg_pattern_address = (u16)(ppu->GetPPUCONT() & 0x10) << 8;
        cartridge_view->CopyPatterns(bg_patterns, bg_pattern_address, 0x1000);

        u8 palette_ram[0x10];
        ppu->CopyPaletteRAM(palette_ram, false);

        // top left screen is fixed
        render_screen(nametable_framebuffer, &vram[0x000], bg_patterns, palette_ram, 0, 0);

        // render the others based on mirroring
        switch(cartridge_view->GetNametableMirroring()) {
        case Systems::NES::MIRRORING_VERTICAL:
            render_screen(nametable_framebuffer, &vram[0x400], bg_patterns, palette_ram, 256,   0);
            render_screen(nametable_framebuffer, &vram[0x000], bg_patterns, palette_ram,   0, 240);
            render_screen(nametable_framebuffer, &vram[0x400], bg_patterns, palette_ram, 256, 240);
            break;

        case Systems::NES::MIRRORING_HORIZONTAL:
            render_screen(nametable_framebuffer, &vram[0x000], bg_patterns, palette_ram, 256,   0);
            render_screen(nametable_framebuffer, &vram[0x400], bg_patterns, palette_ram,   0, 240);
            render_screen(nametable_framebuffer, &vram[0x400], bg_patterns, palette_ram, 256, 240);
            break;

        default:
            break;
        }

        if(show_scroll_window) {
            int scroll_x = ppu->GetScrollX();
            int scroll_y = ppu->GetScrollY();
            int ey = (scroll_y + 239) % 240;
            for(int i = 0; i < 256; i++) {
                int x = (scroll_x + i) & 511;
                nametable_framebuffer[scroll_y * 512 + x] = 0xFF000000;
                nametable_framebuffer[ey * 512 + x] = 0xFF000000;
            }

            int ex = (scroll_x + 256) & 511;
            for(int i = 0; i < 256; i++) {
                int y = (scroll_y + i) % 240;
                nametable_framebuffer[y * 512 + scroll_x] = 0xFF000000;
                nametable_framebuffer[y * 512 + ex] = 0xFF000000;
            }
        }

        // update the opengl texture
        GLuint gl_texture = (GLuint)(intptr_t)nametable_texture;
        glBindTexture(GL_TEXTURE_2D, gl_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 512, 512, GL_RGBA, GL_UNSIGNED_BYTE, nametable_framebuffer);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void PPUState::UpdateSpriteTexture()
{
    auto si = GetMySystemInstance();
    auto ppu = si->GetPPU();
    auto system_view = si->GetMemoryViewAs<Systems::NES::SystemView>();

    // fetch the sprite pattern data in one go
    u8 sprite_patterns[0x1000];
    auto cartridge_view = system_view->GetCartridgeView();
    u16 sprite_pattern_address = (u16)(ppu->GetPPUCONT() & 0x08) << 9;
    cartridge_view->CopyPatterns(sprite_patterns, sprite_pattern_address, 0x1000);

    // grab the PPU OAM
    ppu->CopyOAM(oam_copy);

    // we'll need sprite palettes
    u8 palette_ram[0x10];
    ppu->CopyPaletteRAM(palette_ram, true);

    // render 8x8 sprites
    u8* oam_ptr = oam_copy;
    for(int sprite_y = 0; sprite_y < 8; sprite_y++) {
        int sy = sprite_y * 9; // +1 for the dividing line
        for(int sprite_x = 0; sprite_x < 8; sprite_x++) {
            int sx = sprite_x * 9;

            // fetch sprite components
            u8 y    = *oam_ptr++;
            u8 tile = *oam_ptr++;
            u8 attr = *oam_ptr++;
            u8 x    = *oam_ptr++;

            // parse attr
            u8   pal    = attr & 0x03;
            bool flip_x = attr & 0x40;
            bool flip_y = attr & 0x80;

            // render one 8x8
            for(int i = 0; i < 8; i++) {
                int y = sy + i;

                int use_i = flip_y ? (7 - i) : i; // vertical flip
                u8 byte0 = sprite_patterns[(tile << 4) + use_i];
                u8 byte1 = sprite_patterns[(tile << 4) + use_i + 8];

                for(int j = 0; j < 8; j++) {
                    int x = sx + j;
                    int use_j = flip_x ? (7 - j) : j; // horizontal flip
                    u8 bit0  = (byte0 >> (7 - use_j)) & 0x01;
                    u8 bit1  = (byte1 >> (7 - use_j)) & 0x01;
                    u8 index = (pal << 2) | (bit1 << 1) | bit0;
                    u8 color = palette_ram[index];

                    sprites_framebuffer[y * 128 + x] = 0xFF000000 | Systems::NES::rgb_palette_map[color];
                }
            }
        }
    }

    // update the opengl texture
    GLuint gl_texture = (GLuint)(intptr_t)sprites_texture;
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, sprites_framebuffer);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void PPUState::UpdatePatternTextures()
{
    auto si = GetMySystemInstance();
    auto ppu = si->GetPPU();
    auto system_view = si->GetMemoryViewAs<Systems::NES::SystemView>();
    auto cartridge_view = system_view->GetCartridgeView();

    // we'll need both palettes
    ppu->CopyPaletteRAM(&palette_copy[0x00], false);
    ppu->CopyPaletteRAM(&palette_copy[0x10], true);

    // loop over both pattern tables
    for(int pt = 0; pt < 2; pt++) {
        // fetch the pattern data in one go
        u8 patterns[0x1000];
        cartridge_view->CopyPatterns(patterns, pt << 12, 0x1000);

        // render 16x16 tiles
        u8* pattern_ptr = patterns;
        for(int tile_y = 0; tile_y < 16; tile_y++) {
            int ty = tile_y * 8; // +1 for the dividing line
            for(int tile_x = 0; tile_x < 16; tile_x++) {
                int tx = tile_x * 8;

                // render one 8x8 tile
                for(int i = 0; i < 8; i++) {
                    int y = ty + i;

                    u8 byte0 = *(pattern_ptr + 0);
                    u8 byte1 = *(pattern_ptr + 8);
                    pattern_ptr++;

                    for(int j = 0; j < 8; j++) {
                        int x = tx + j;
                        u8 bit0  = (byte0 >> (7 - j)) & 0x01;
                        u8 bit1  = (byte1 >> (7 - j)) & 0x01;
                        u8 index = (palette_index << 2) | (bit1 << 1) | bit0;
                        u8 color = palette_copy[index];

                        pattern_framebuffer[pt][y * 128 + x] = 0xFF000000 | Systems::NES::rgb_palette_map[color];
                    }
                }
            }
        }

        // update the opengl texture
        GLuint gl_texture = (GLuint)(intptr_t)pattern_texture[pt];
        glBindTexture(GL_TEXTURE_2D, gl_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, pattern_framebuffer[pt]);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}


bool PPUState::SaveWindow(std::ostream& os, std::string& errmsg)
{
    WriteVarInt(os, display_mode);
    WriteVarInt(os, (int)show_scroll_window);
    if(!os.good()) {
        errmsg = "Error in " + WindowPrefix();
        return false;
    }
    return true;
}

bool PPUState::LoadWindow(std::istream& is, std::string& errmsg)
{
    display_mode = ReadVarInt<int>(is);
    show_scroll_window = (bool)ReadVarInt<int>(is);
    if(!is.good()) {
        errmsg = "Error in " + WindowPrefix();
        return false;
    }
    return true;
}

std::shared_ptr<Watch> Watch::CreateWindow()
{
    return make_shared<Watch>();
}

Watch::Watch()
    : BaseWindow()
{
    SetTitle("Watch 1");
}

Watch::~Watch()
{
}

void Watch::CheckInput()
{
    if(ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        cout << WindowPrefix() << "CheckInput" << endl;
    }

    if(ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if(selected_row >= 0 && selected_row < watches.size()) {
            watches.erase(watches.begin() + selected_row, watches.begin() + selected_row + 1);

            // gotta re-fill the sorted_watches array
            sorted_watches.clear();
            for(int i = 0; i < watches.size(); i++) {
                sorted_watches.push_back(i);
            }
            need_resort = true;
        }
    }
}

void Watch::Update(double deltaTime)
{
    if(need_resort) {
        Resort();
        need_resort = false;
    }
}

void Watch::Resort()
{
    if(sort_column == -1) { // no sort!
        sort(sorted_watches.begin(), sorted_watches.end());
        return;
    }

    // otherwise, special sort!
    sort(sorted_watches.begin(), sorted_watches.end(), [&](int const& a, int const& b)->bool {
        bool diff;

        auto ap = watches[a];
        auto bp = watches[b];

        if(sort_column == 0) {
            stringstream a_ss, b_ss;
            a_ss << *ap->expression;
            b_ss << *bp->expression;

            if(reverse_sort) diff = b_ss.str() <= a_ss.str();
            else             diff = a_ss.str() <= b_ss.str();
        } else {
            if(reverse_sort) diff = bp->last_value <= ap->last_value;
            else             diff = ap->last_value <= bp->last_value;
        } 

        return diff;
    });
}

void Watch::Render()
{
    ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoBordersInBodyUntilResize
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchSame
        | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(-1, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(-1, 0));

    // We use nested tables so that each row can have its own layout. This will be useful when we can render
    // things like plate comments, labels, etc
    if(ImGui::BeginTable("watch_table", 2, table_flags)) {
        ImGui::TableSetupColumn("Expression", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
        ImGui::TableSetupColumn("Value"     , ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);
        ImGui::TableHeadersRow();

        // Sort our data (on the next frame) if sort specs have been changed!
        if(ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs(); sort_specs && sort_specs->SpecsDirty) {
            if(auto spec = &sort_specs->Specs[0]) {
                sort_column = spec->ColumnUserID;
                reverse_sort = (spec->SortDirection == ImGuiSortDirection_Descending);
            } else { // no sort!
                sort_column = -1;
                reverse_sort = false;
            }

            need_resort = true;
            sort_specs->SpecsDirty = false;
        }

        for(int row = 0; row < sorted_watches.size(); row++) {
            auto watch_index = sorted_watches[row];
            auto& watch_data = watches[watch_index];

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // show selection even when editing
            ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
            char buf[32];
            sprintf(buf, "##watch_selectable_row%d", row);
            if(ImGui::Selectable(buf, selected_row == watch_index, selectable_flags)) {
                selected_row = watch_index;
            }

            ImGui::SameLine();
            if(editing == row) {
                ImGui::PushItemWidth(-FLT_MIN);
                if(ImGui::InputText("", &edit_string, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    do_set_watch = true;
                }

                // if we just started editing, focus on the input text item
                if(started_editing) {
                    ImGui::SetKeyboardFocusHere(-1);
                    // wait until item is activated
                    if(ImGui::IsItemActive()) started_editing = false;
                } else if(!do_set_watch && !ImGui::IsItemActive()) { // check if item lost activation
                    // stop editing without saving
                    editing = -1;
                }
            } else {
                // format the expression..we can't use an expression string cache because dynamic elements can change (labels)
                stringstream ss;
                ss << *watch_data->expression;

                if(ImGui::IsItemHovered()) {
                    if(ImGui::IsMouseDoubleClicked(0)) {
                        editing = row;
                        edit_string = ss.str();
                        started_editing = true;
                    } else if(ImGui::IsMouseClicked(1)) {
                        selected_row = watch_index;
                        ImGui::OpenPopup("watch_context_menu");
                    }
                }

                ImGui::Text("%s", ss.str().c_str());
            }

            // evaluate and display the expression, caching the value for sort only
            ImGui::TableNextColumn();
            s64 result;
            string errmsg;
            if(watch_data->expression->Evaluate(&result, errmsg)) {
                watch_data->last_value = result;

                char const* fmt = nullptr;

                if(watch_data->base == 2) {
                    stringstream ss;
                    switch(watch_data->data_type) {
                    case WatchData::DataType::BYTE:
                        ss << bitset<8>(result);
                        break;

                    case WatchData::DataType::WORD:
                        ss << bitset<16>(result);
                        break;

                    case WatchData::DataType::LONG:
                    case WatchData::DataType::FLOAT32:
                        ss << bitset<32>(result);
                        break;
                    }

                    auto str = ss.str();
                    // drop leading 0s if pad is disabled
                    if(!watch_data->pad) str.erase(0, str.find_first_not_of('0'));
                    ImGui::Text("%%%s", str.c_str());
                } else if(watch_data->base == 10) {
                    if(watch_data->data_type == WatchData::DataType::FLOAT32) {
                        float fval = *(float*)&result;
                        ImGui::Text("%f", fval);
                    } else {
                        fmt = "%d";
                    }
                } else if(watch_data->base == 16) {
                    switch(watch_data->data_type) {
                    case WatchData::DataType::BYTE:
                        if(watch_data->pad) fmt = "$%02X";
                        else                fmt = "$%X";
                        break;

                    case WatchData::DataType::WORD:
                        if(watch_data->pad) fmt = "$%04X";
                        else                fmt = "$%X";
                        break;

                    case WatchData::DataType::LONG:
                    case WatchData::DataType::FLOAT32:
                        if(watch_data->pad) fmt = "$%08X";
                        else                fmt = "$%X";
                        break;

                    default:
                        fmt = nullptr;
                    }
                }

                if(fmt != nullptr) ImGui::Text(fmt, result);
            } else {
                ImGui::TextDisabled("%s", errmsg.c_str());
                watch_data->last_value = 0;
            }
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("<New>");
        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            // create a new empty expression and start editing it. place it at the end of the sorted list but don't re-sort yet
            auto watch_data = make_shared<WatchData>();
            watch_data->expression = make_shared<Systems::NES::Expression>();

            watches.push_back(watch_data);
            sorted_watches.push_back(watches.size() - 1);

            editing = sorted_watches.size() - 1;
            edit_string = "";
            started_editing = true;
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar(2);

    if(ImGui::BeginPopupContextItem("watch_context_menu")) {
        if(selected_row >= 0 && selected_row < watches.size()) {
            auto& watch_data = watches[selected_row];
            if(ImGui::BeginMenu("Display")) {
                if(ImGui::MenuItem("Byte", nullptr, watch_data->data_type == WatchData::DataType::BYTE)) {
                    watch_data->data_type = WatchData::DataType::BYTE;
                    SetDereferenceOp(watch_data);
                }
                if(ImGui::MenuItem("Word", nullptr, watch_data->data_type == WatchData::DataType::WORD)) {
                    watch_data->data_type = WatchData::DataType::WORD;
                    SetDereferenceOp(watch_data);
                }
                if(ImGui::MenuItem("Long", nullptr, watch_data->data_type == WatchData::DataType::LONG)) {
                    watch_data->data_type = WatchData::DataType::LONG;
                    SetDereferenceOp(watch_data);
                }
                if(ImGui::MenuItem("Float", nullptr, watch_data->data_type == WatchData::DataType::FLOAT32)) {
                    watch_data->data_type = WatchData::DataType::FLOAT32;
                    SetDereferenceOp(watch_data);
                }
                if(ImGui::MenuItem("User TODO", nullptr, false)) {
                }
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Format")) {
                if(ImGui::MenuItem("Binary", nullptr, watch_data->base == 2)) {
                    watch_data->base = 2;
                }
                if(ImGui::MenuItem("Decimal", nullptr, watch_data->base == 10)) {
                    watch_data->base = 10;
                }
                if(ImGui::MenuItem("Octal", nullptr, watch_data->base == 8)) {
                    watch_data->base = 8;
                }
                if(ImGui::MenuItem("Hexadecimal", nullptr, watch_data->base == 16)) {
                    watch_data->base = 16;
                }
                ImGui::EndMenu();
            }
            if(ImGui::MenuItem("Pad display", nullptr, watch_data->pad)) {
                watch_data->pad = !watch_data->pad;
            }
        }
        ImGui::EndPopup();
    }

    // try setting the watch or inform the user of errors
    if(do_set_watch) SetWatch();
}

void Watch::CreateWatch(string const& expression_string)
{
    // create a new empty expression and start editing it. place it at the end of the sorted list but don't re-sort yet
    auto watch_data = make_shared<WatchData>();
    watch_data->expression = make_shared<Systems::NES::Expression>();
    
    watches.push_back(watch_data);
    sorted_watches.push_back(watches.size() - 1);
    
    editing = sorted_watches.size() - 1;
    edit_string = expression_string;
    
    SetWatch();
}

void Watch::SetWatch()
{
    if(!wait_dialog) {
        auto& watch_index = sorted_watches[editing];
        auto& watch_data = watches[watch_index];
        auto& expr = watch_data->expression;

        string errmsg;
        int errloc;

        // try parsing the expression first
        if(expr->Set(edit_string, errmsg, errloc, false)) {
            errloc = -1;

            // apply SystemInstance expression changes to the expression before System
            // so that we can convert names like "X" and "PC" to their actual state values
            // at the time the expression is evaluated
            if(GetMySystemInstance()->FixupExpression(expr, errmsg)) {
                // expression was valid from a grammar point of view, now apply semantics
                // allow labels, defines, derefs, and enums, but not addressing modes
                FixupFlags fixup_flags = FIXUP_DEFINES | FIXUP_LABELS | FIXUP_DEREFS | FIXUP_ENUMS;
                if(GetSystem()->FixupExpression(expr, errmsg, fixup_flags)) {
                    // Expression contained valid elements, now DereferenceOp nodes need evaluation functions set
                    if(SetDereferenceOp(watch_data)) {
                        // success, done editing and re-sort after adding
                        do_set_watch = false;
                        editing = -1;
                        need_resort = true;
                    }
                }
            }
        }

        // if we didn't finish editing there was an error...
        if(editing != -1) {
            stringstream ss;
            ss << "There was a problem parsing the expression: " << errmsg;
            if(errloc >= 0) ss << " (at offset " << errloc << ")";
            set_watch_error_message = ss.str();
            wait_dialog = true;
        }
    } 

    if(wait_dialog) {
        if(GetMainWindow()->OKPopup("Expression error", set_watch_error_message)) {
            wait_dialog = false;
            do_set_watch = false;
            started_editing = true; // re-edit the expression
        }
    }
}

bool Watch::SetDereferenceOp(std::shared_ptr<WatchData> const& watch_data)
{
    ExploreData ed = {
        .watch_data = watch_data
    };

    // DereferenceOp nodes need evaluation functions set and we can use Explore() to find them
    auto cb = std::bind(&Watch::ExploreCallback, this, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4);
    return watch_data->expression->Explore(cb, (void*)&ed);
}

bool Watch::ExploreCallback(shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void* userdata)
{
    ExploreData* ed = (ExploreData*)userdata;

    if(auto deref = dynamic_pointer_cast<BaseExpressionNodes::DereferenceOp>(node)) {
        BaseExpressionNodes::DereferenceOp::dereference_func_t f;

        switch(ed->watch_data->data_type) {
        case WatchData::DataType::BYTE:
            f = std::bind(&Watch::DereferenceByte, this, placeholders::_1, placeholders::_2, placeholders::_3);
            break;
        case WatchData::DataType::WORD:
            f = std::bind(&Watch::DereferenceWord, this, placeholders::_1, placeholders::_2, placeholders::_3);
            break;
        case WatchData::DataType::LONG:
        case WatchData::DataType::FLOAT32:
            f = std::bind(&Watch::DereferenceLong, this, placeholders::_1, placeholders::_2, placeholders::_3);
            break;
        default:
            assert(false); // TODO WORD, custom types with treenodes
            break;
        }

        deref->SetDereferenceFunction(f);
    }

    return true;
}

bool Watch::DereferenceByte(s64 in, s64* out, string& errmsg)
{
    auto memory_view = GetMySystemInstance()->GetMemoryView();
    if(!memory_view) {
        errmsg = "Internal error";
        return false;
    }

    // TODO would be cool to support banks within the address itself
    // shouldn't be too difficult. Overload Peek() to take a GlobalMemoryLocation
    // and build the memory location here
    *out = memory_view->Peek(in);
    return true;
}

bool Watch::DereferenceWord(s64 in, s64* out, string& errmsg)
{
    auto memory_view = GetMySystemInstance()->GetMemoryView();
    if(!memory_view) {
        errmsg = "Internal error";
        return false;
    }

    *out = (u16)memory_view->Peek(in) | ((u16)memory_view->Peek(in + 1) << 8);
    return true;
}

bool Watch::DereferenceLong(s64 in, s64* out, string& errmsg)
{
    auto memory_view = GetMySystemInstance()->GetMemoryView();
    if(!memory_view) {
        errmsg = "Internal error";
        return false;
    }

    *out = (u32)memory_view->Peek(in) | ((u32)memory_view->Peek(in + 1) << 8)
           | ((u32)memory_view->Peek(in + 2) << 16)| ((u32)memory_view->Peek(in + 3) << 24);
    return true;
}

bool Watch::SaveWindow(std::ostream& os, std::string& errmsg)
{
    WriteVarInt(os, watches.size());
    for(auto& watch_data : watches) {
        if(!watch_data->Save(os, errmsg)) return false;
    }
    return true;
}

bool Watch::LoadWindow(std::istream& is, std::string& errmsg)
{
    auto si = GetMySystemInstance();

    watches.clear();
    sorted_watches.clear();

    int watch_count = ReadVarInt<int>(is);
    for(int i = 0; i < watch_count; i++) {
        auto watch_data = make_shared<WatchData>();
        if(!watch_data->Load(is, errmsg)) return false;
        // dereference ops aren't saved with the expression and need to be reconfigured
        SetDereferenceOp(watch_data);
        // same for SystemInstanceState nodes
        if(!si->FixupExpression(watch_data->expression, errmsg)) return false;
        watches.push_back(watch_data);
        sorted_watches.push_back(i);
    }

    need_resort = true;
    return true;
}

bool Watch::WatchData::Save(std::ostream& os, std::string& errmsg) const
{
    if(!expression->Save(os, errmsg)) return false;
    WriteVarInt(os, last_value);
    WriteEnum(os, data_type);
    WriteVarInt(os, (int)pad);
    WriteVarInt(os, base);
    if(!os.good()) {
        errmsg = "Error writing WatchData";
        return false;
    }
    return true;
}

bool Watch::WatchData::Load(std::istream& is, std::string& errmsg)
{
    expression = make_shared<Systems::NES::Expression>();
    if(!expression->Load(is, errmsg)) return false;
    last_value = ReadVarInt<s64>(is);
    data_type = ReadEnum<DataType>(is);
    pad = (bool)ReadVarInt<int>(is);
    base = ReadVarInt<int>(is);
    if(!is.good()) {
        errmsg = "Error loading WatchData";
        return false;
    }

    // need to call Update() on all labels
    auto cb = [&](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
        if(auto label_node = dynamic_pointer_cast<Systems::NES::ExpressionNodes::Label>(node)) {
            label_node->Update();
        }
        return true;
    };

    return expression->Explore(cb, nullptr);
}

std::shared_ptr<Breakpoints> Breakpoints::CreateWindow()
{
    return make_shared<Breakpoints>();
}

Breakpoints::Breakpoints()
    : BaseWindow()
{
    SetTitle("Breakpoints");

}

Breakpoints::~Breakpoints()
{
}

void Breakpoints::CheckInput()
{
    if(ImGui::IsKeyPressed(ImGuiKey_Delete) && selected_breakpoint) {
        GetMySystemInstance()->ClearBreakpoint(selected_key, selected_breakpoint);
        selected_breakpoint = nullptr;
    }
}

void Breakpoints::Update(double deltaTime)
{
}

void Breakpoints::Render()
{
    bool has_context = false;

    ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoBordersInBodyUntilResize
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(-1, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(-1, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    // We use nested tables so that each row can have its own layout. This will be useful when we can render
    // things like plate comments, labels, etc
    if(ImGui::BeginTable("breakpoints_table", 4, table_flags)) {
        ImGui::TableSetupColumn("##En"      , ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Type"      , ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Location"  , ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Condition" , ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        // loop over the breakpoints and render a row for each
        int row = 0;
        GetMySystemInstance()->IterateBreakpoints([&](SystemInstance::breakpoint_key_t const& key, shared_ptr<BreakpointInfo> const& bpi) {
            ImGui::PushID(row);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // show selection even when editing
            ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
            if(ImGui::Selectable("##selectable", selected_row == row, selectable_flags) || (selected_row == row)) { // updates selected_breakpoint after previous one is deleted
                selected_row = row;
                selected_key = key;
                selected_breakpoint = bpi;
            }

            if(ImGui::IsItemHovered()) {
                // IsItemHovered() will only be true if the popup isn't open, so we can safely change context_breakpoint
                context_breakpoint = bpi;
                has_context = true;

                // when the user activates a breakpoint, go to it in the listing window
                if(ImGui::IsMouseDoubleClicked(0)) {
                    if(auto listing = GetMyListing()) {
                        listing->GoToAddress(bpi->address, true);
                    }
                }
            }

            ImGui::SameLine();
            ImGui::Checkbox("", &bpi->enabled);

            ImGui::TableNextColumn();
            ImGui::Text(bpi->address.is_chr ? "CHR:" : "CPU:");

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
            if(!bpi->address.is_chr) {
                ImGui::SameLine(); ImGuiFlagButton(&bpi->break_read   , "R", "Break on read");
                ImGui::SameLine(); ImGuiFlagButton(&bpi->break_write  , "W", "Break on write");
                ImGui::SameLine(); ImGuiFlagButton(&bpi->break_execute, "X", "Break on execute");
            }
            ImGui::PopStyleVar(1);

            // format address
            ImGui::TableNextColumn();
            stringstream ss;
            bpi->address.FormatAddress(ss, true, bpi->has_bank);
            ImGui::Text(ss.str().c_str());

            // format condition
            ImGui::TableNextColumn();
            if(bpi == editing_breakpoint_info && editing == EditMode::CONDITION) {
                ImGui::PushItemWidth(-FLT_MIN);
                if(ImGui::InputText("##edit_condition", &edit_string, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    do_set_breakpoint = true;
                }

                // if we just started editing, focus on the input text item
                if(started_editing) {
                    ImGui::SetKeyboardFocusHere(-1);
                    // wait until item is activated
                    if(ImGui::IsItemActive()) started_editing = false;
                } else if(!do_set_breakpoint && !ImGui::IsItemActive()) { // check if item lost activation
                    // stop editing without saving
                    editing = EditMode::NONE;
                    editing_breakpoint_info = nullptr;
                }
            } else {
                stringstream ss;
                if(bpi->condition) ss << *bpi->condition;
                ImGui::Text("%s", ss.str().c_str());
            }

            ImGui::PopID();
            row++;
        });

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        // render the <New> row...
        ImGui::PushID(row);
        ImGui::TableNextRow();

        ImGui::TableNextColumn(); // checkbox enabled

        ImGui::TableNextColumn(); // rwx

        ImGui::TableNextColumn(); // address

        if(editing == EditMode::ADDRESS) {
            ImGui::PushItemWidth(-FLT_MIN);
            if(ImGui::InputText("##edit_address", &edit_string, ImGuiInputTextFlags_EnterReturnsTrue)) {
                do_set_breakpoint = true;
            }

            // if we just started editing, focus on the input text item
            if(started_editing) {
                ImGui::SetKeyboardFocusHere(-1);
                // wait until item is activated
                if(ImGui::IsItemActive()) started_editing = false;
            } else if(!do_set_breakpoint && !ImGui::IsItemActive()) { // check if item lost activation
                // stop editing without saving
                editing = EditMode::NONE;
                editing_breakpoint_info = nullptr;
            }
        } else {
            ImGui::TextDisabled("<New>");
            if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                // create a new disabled breakpoint and start editing it
                editing_breakpoint_info = make_shared<BreakpointInfo>();
                editing_breakpoint_info->break_read = true;
                editing_breakpoint_info->break_write = true;
                editing_breakpoint_info->break_execute = true;
                edit_string = "";
                editing = EditMode::ADDRESS;
                started_editing = true;
            }
        }

        ImGui::TableNextColumn(); // condition

        ImGui::PopID();

        ImGui::EndTable();
    }

    ImGui::PopStyleVar(3);

    if(context_breakpoint && ImGui::BeginPopupContextItem("breakpoint_context_menu")) {
        if(ImGui::MenuItem("Edit Condition")) {
            editing_breakpoint_info = context_breakpoint;

            // initialize the condition string
            stringstream ss;
            if(editing_breakpoint_info->condition) ss << *editing_breakpoint_info->condition;
            edit_string = ss.str();

            editing = EditMode::CONDITION;
            started_editing = true;
        }
        ImGui::EndPopup();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // if there's no hovered item and the popup isn't open, clear context_breakpoint so we don't open the context
    // menu on non-valid entries
    if(!has_context && !ImGui::IsPopupOpen("breakpoint_context_menu")) {
        context_breakpoint = nullptr;
    }

    if(do_set_breakpoint) {
        if(editing == EditMode::ADDRESS) SetBreakpoint();
        else if(editing == EditMode::CONDITION) SetCondition();
    }
}

// Try to set the current edit_string as the breakpoint info's address
// If you want to specify the bank, type in $bbAAAA
void Breakpoints::SetBreakpoint()
{
    if(!wait_dialog) {
        string errmsg;
        int errloc;

        // try parsing the expression first
        auto expr = make_shared<Systems::NES::Expression>();
        if(expr->Set(edit_string, errmsg, errloc, false)) {
            errloc = -1;

            // before we go to System::FixupExpression, we need to convert some Names to SystemInstanceStates
            if(GetMySystemInstance()->FixupExpression(expr, errmsg)) {
                // expression was valid from a grammar point of view, now apply semantics
                // allow labels, defines, no derefs, no modes
                FixupFlags fixup_flags = FIXUP_DEFINES | FIXUP_LABELS | FIXUP_ENUMS | FIXUP_LONG_LABELS;
                if(GetSystem()->FixupExpression(expr, errmsg, fixup_flags)) {
                    // Expression contained valid elements, evaluate the function to determine where the breakpoint should be
                    s64 result;
                    if(expr->Evaluate(&result, errmsg)) {
                        // result contains the address of our breakpoint!
                        if(result < 0) {
                            errmsg = "Invalid address";
                        } else if(result < 0x10000) { // no bank specified
                            editing_breakpoint_info->address = {
                                .address = (u16)(result & 0xFFFF),
                                .is_chr = false,
                                .prg_rom_bank = 0,
                            };


                            editing_breakpoint_info->has_bank = false;
                            editing_breakpoint_info->enabled = true;

                            // set the u16 style breakpoint
                            GetMySystemInstance()->SetBreakpoint((u16)(result & 0xFFFF), editing_breakpoint_info);
                            editing_breakpoint_info = nullptr;
                        } else { // user specified a bank via a label or manually
                            // use the bank byte to build a GlobalMemoryLocation and make sure it's valid
                            editing_breakpoint_info->address = {
                                .address      = (u16)(result & 0xFFFF),
                                .is_chr       = false,
                                .prg_rom_bank = (u16)((result >> 16) & 0xFF),
                            };

                            editing_breakpoint_info->has_bank = true;
                            editing_breakpoint_info->enabled = true;

                            if(!GetSystem()->GetMemoryObject(editing_breakpoint_info->address)) {
                                stringstream ss;
                                ss << "Invalid address (no memory exists at $" << hex << uppercase << setw(4) 
                                   << setfill('0') << result << ")";
                                errmsg = ss.str();
                            } else {
                                // valid target
                                GetMySystemInstance()->SetBreakpoint(editing_breakpoint_info->address, editing_breakpoint_info);
                                editing_breakpoint_info = nullptr;
                            }
                        }

                        if(!editing_breakpoint_info) {
                            // success, done editing
                            do_set_breakpoint = false;
                            editing = EditMode::NONE;
                        }
                    }
                }
            }
        }

        // if we didn't finish editing there was an error...
        if(editing != EditMode::NONE) {
            stringstream ss;
            ss << "There was a problem setting the breakpoint: " << errmsg;
            if(errloc >= 0) ss << " (at offset " << errloc << ")";
            set_breakpoint_error_message = ss.str();
            wait_dialog = true;
        }
    } 

    if(wait_dialog) {
        if(GetMainWindow()->OKPopup("Expression error", set_breakpoint_error_message)) {
            wait_dialog = false;
            do_set_breakpoint = false;
            started_editing = true; // re-edit the expression
        }
    }
}

void Breakpoints::SetCondition()
{
    if(!wait_dialog) {
        string errmsg;
        int errloc;

        // try parsing the expression first
        auto expr = make_shared<Systems::NES::Expression>();
        if(expr->Set(edit_string, errmsg, errloc, false)) {
            errloc = -1;

            // Expression contained valid elements, try setting the condition
            if(GetMySystemInstance()->SetBreakpointCondition(editing_breakpoint_info, expr, errmsg)) {
                // success, done editing
                editing_breakpoint_info = nullptr;
                do_set_breakpoint = false;
                editing = EditMode::NONE;
            }
        }

        // if we didn't finish editing there was an error...
        if(editing != EditMode::NONE) {
            stringstream ss;
            ss << "There was a problem setting the condition: " << errmsg;
            if(errloc >= 0) ss << " (at offset " << errloc << ")";
            set_breakpoint_error_message = ss.str();
            wait_dialog = true;
        }
    } 

    if(wait_dialog) {
        if(GetMainWindow()->OKPopup("Expression error", set_breakpoint_error_message)) {
            wait_dialog = false;
            do_set_breakpoint = false;
            started_editing = true; // re-edit the expression
        }
    }
}

std::shared_ptr<Memory> Memory::CreateWindow()
{
    return make_shared<Memory>();
}

Memory::Memory()
    : BaseWindow()
{
    SetTitle("Memory");
    SetNoScrollbar(true);

    tile_display_framebuffer = new u32[256 * 256];
}

Memory::~Memory()
{
    delete [] tile_display_framebuffer;
}

void Memory::CheckInput()
{
}

void Memory::Update(double deltaTime)
{
}

void Memory::Render()
{
    auto si = GetMySystemInstance();
    auto system_view = si->GetMemoryViewAs<Systems::NES::SystemView>();
    if(!system_view) return;

    auto ppu_view = system_view->GetPPUView();
    if(!ppu_view) return;

    auto cartridge = GetSystem()->GetCartridge();
    if(!cartridge) return;

    RenderAddressBar();

    ImGui::SameLine();
    ImGuiFlagButton(&show_tile_display, "T", "Show 2bpp Tile Display");

    if(ImGui::SameLine(); ImGuiFlagButton(nullptr, "+", "Increase offset by 1")) {
        memory_shift = (memory_shift + 1) & 15;
    }

    if(ImGui::SameLine(); ImGuiFlagButton(nullptr, "-", "Decrease offset by 1")) {
        memory_shift = (memory_shift + 15) & 15;
    }

    bool visible_table = true;
    if(show_tile_display) {
        auto size = ImGui::GetWindowSize();
        size.x *= 0.75;
        visible_table = ImGui::BeginChild("left", size);
    }

    GlobalMemoryLocation min_visible_address{};
    bool first_visible_address = true;

    ImGuiTableFlags table_flags = ImGuiTableFlags_NoBordersInBodyUntilResize
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchSame;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(-1, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(-1, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    // We use nested tables so that each row can have its own layout. This will be useful when we can render
    // things like plate comments, labels, etc
    static int data_columns[] = { 16, 8, 4, 4 }; // byte, word, long, float
    if(visible_table && ImGui::BeginTable("memory_table", 1 + data_columns[memory_size], table_flags)) {
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed, -1);

        char colhdr[3] = "0_";
        for(int i = 0; i < 16; i++) {
            int si = i + memory_shift;
            if(si >= 16) {
                colhdr[0] = '1';
                si -= 16;
            }
            colhdr[1] = (si < 10) ? ('0' + si) : ('A' + si - 10);
            ImGui::TableSetupColumn(colhdr, ImGuiTableColumnFlags_WidthStretch);

            // skip some columns for larger data sizes
            if(memory_size == 1) i += 1;
            else if(memory_size > 1) i += 3;
        }

        ImGui::TableHeadersRow();

        u32 total_rows = 0;
        if(memory_mode == 0) { // CPU
            total_rows = 0x10000 >> 4; // 16 bytes per row, 64KiB of memory
        } else if(memory_mode == 1) { // PPU
            total_rows = 0x4000 >> 4; // 16KiB address space
        } else if(memory_mode == 2) { // PRG
            // 16KiB per PRG-ROM bank
            u32 prg_size = GetSystem()->GetCartridge()->header.num_prg_rom_banks * 0x4000;
            if(prg_size != 0) total_rows = prg_size >> 4;
        } else if(memory_mode == 3) { // CHR
            int num_chr_rom_banks = GetSystem()->GetCartridge()->header.num_chr_rom_banks;
            // 8KiB per CHR-ROM bank
            u32 chr_size = num_chr_rom_banks * 0x2000;
            if(chr_size != 0) total_rows = chr_size >> 4;
        }

        ImGuiListClipper clipper;
        clipper.Begin(total_rows);

        int go_to_address_row = (go_to_address >> 4);
        if(go_to_address >= 0) {
            clipper.ForceDisplayRangeByIndices(go_to_address_row - 1, go_to_address_row + 1);
        }

        while(clipper.Step()) {
            for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                stringstream ss;
                GlobalMemoryLocation address{};
                bool format_bank = false;
                u8 data[16];
                int empty_after = 16;
                for(int i = 0; i < 16; i++) {
                    switch(memory_mode) {
                    case 0: // CPU
                        if(i == 0) {
                            address.address = (u16)(row << 4) + memory_shift;
                        }
                        data[i] = system_view->Peek(address.address + i);
                        if(address.address + i >= 0x10000) {
                            empty_after = (i < empty_after) ? i : empty_after;
                        }
                        break;

                    case 1: // PPU
                        if(i == 0) {
                            address.address = (u16)(row << 4) + memory_shift;
                            address.is_chr = true;
                        }
                        data[i] = ppu_view->PeekPPU(address.address + i);
                        if(address.address + i >= 0x4000) {
                            empty_after = (i < empty_after) ? i : empty_after;
                        }
                        break;

                    case 2: { // PRG-ROM
                        if(i == 0) {
                            // 0x4000 bytes per bank, 0x400 rows per bank in this table
                            // every (1<<10) rows increases the bank number
                            address.address = (u16)((row & 0x03FF) << 4) + memory_shift;
                            address.prg_rom_bank = (u16)(row >> 10);
                            format_bank = true;
                        }
                        u16 bank = address.prg_rom_bank;
                        u16 src = address.address + i;
                        if(src >= 0x4000) { // wrap into next bank
                            bank += 1;
                            src &= 0x3FFF;
                        }
                        if(bank < cartridge->header.num_prg_rom_banks) {
                            data[i] = cartridge->ReadProgramRomRelative(bank, src);
                        } else {
                            empty_after = (i < empty_after) ? i : empty_after;
                        }
                        break;
                    }

                    case 3: // CHR-ROM
                        if(i == 0) {
                            // 0x2000 bytes per bank, 0x400 rows per bank in this table
                            // every (1<<9) rows increases the bank number
                            address.address = (u16)((row & 0x01FF) << 4) + memory_shift;
                            address.chr_rom_bank = (u16)(row >> 9);
                            format_bank = true;
                        }
                        u16 bank = address.chr_rom_bank;
                        u16 src = address.address + i;
                        if(src >= 0x2000) { // wrap into next bank
                            bank += 1;
                            src &= 0x1FFF;
                        }
                        if(bank < cartridge->header.num_chr_rom_banks) {
                            data[i] = cartridge->ReadCharacterRomRelative(bank, src);
                        } else {
                            empty_after = (i < empty_after) ? i : empty_after;
                        }
                        break;
                    }
                }

                address.FormatAddress(ss, true, format_bank);
                if(!format_bank) ss << "    "; // hack to get Imgui to set the first column width
                ImGui::Text("%s", ss.str().c_str());
                bool visible = ImGui::IsItemVisible();
                
                if(visible) {
                    if(first_visible_address || address < min_visible_address) {
                        min_visible_address = address;
                        first_visible_address = false;
                    }
                }

                char buf[64];
                for(int i = 0; i < 16;) {
                    int save_i = i;

                    ImGui::TableNextColumn();
                    switch(memory_size) {
                    case 0: // byte
                        snprintf(buf, sizeof(buf), "%02X##%d_%d", data[i], row, i);
                        i += 1;
                        break;
                    case 1: // word
                        snprintf(buf, sizeof(buf), "%04X##%d_%d", (u16)data[i] | ((u16)data[i+1] << 8), row, i);
                        i += 2;
                        break;
                    case 2: { // long
                        u32 v = (u32)data[i] | ((u32)data[i+1] << 8) | ((u32)data[i+2] << 16) | ((u32)data[i+3] << 24);
                        snprintf(buf, sizeof(buf), "%08X##%d_%d", v, row, i);
                        i += 4;
                        break;
                    }
                    case 3: { // float
                        float v = *(float *)(&data[i]);
                        snprintf(buf, sizeof(buf), "%f##%d_%d", v, row, i);
                        i += 4;
                        break;
                    }
                    }

                    // overwrite buffer on empty space
                    if(save_i >= empty_after) {
                        snprintf(buf, sizeof(buf), "##%d_%d", row, i);
                    }

                    // prg/chr bank is 0 for CPU/PPU modes
                    int long_address = address.address + save_i 
                                       + 0x4000 * address.prg_rom_bank 
                                       + 0x2000 * address.chr_rom_bank;
                    if(ImGui::Selectable(buf, long_address == selected_address, ImGuiSelectableFlags_AllowItemOverlap)) {
                        selected_address = long_address;
                    }
                }

                if(go_to_address >= 0 && row == go_to_address_row) {
                    selected_address = go_to_address;

                    if(visible) go_to_address = -1;
                    else        ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleCenterY);
                }
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar(3);

    if(show_tile_display) {
        ImGui::EndChild();
        ImGui::SameLine();
        if(ImGui::BeginChild("right") && !first_visible_address) RenderTileDisplay(min_visible_address);
        ImGui::EndChild();
    }
}

void Memory::RenderAddressBar()
{
    auto width = ImGui::GetWindowSize().x;

    char const * const tooltip = 
        "CPU: system bus memory\n"
        "PPU: VRAM memory\nPRG: PRG-ROM on the cartridge\n"
        "CHR: CHR-ROM on the cartridge\n";
    ImGui::SetNextItemWidth(width * 0.1f);
    ImGui::Combo("##mode", &memory_mode, "CPU\0PPU\0PRG\0CHR\0\0");
    if(ImGui::IsItemHovered()) ImGui::SetTooltip(tooltip);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(width * 0.1f);
    ImGui::Combo("##size", &memory_size, "Byte\0Word\0Long\0Float\0\0");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(width * 0.6f);
    if(ImGui::InputText("Address", &address_text, ImGuiInputTextFlags_EnterReturnsTrue)) {
        set_address = true;
    }

    if(wait_dialog) {
        if(GetMainWindow()->OKPopup("Address error", address_error)) {
            wait_dialog = false;
        }
        return;
    }

    if(!set_address) return;

    auto expr = make_shared<Systems::NES::Expression>();
    int errloc;
    string errmsg;
    // check if expression is valid
    if(expr->Set(address_text, errmsg, errloc, false)) {
        // fixup the expression, allowing labels, defines, derefs, no modes, long labels
        FixupFlags fixup_flags = FIXUP_DEFINES | FIXUP_LABELS | FIXUP_ENUMS | FIXUP_DEREFS
                                 | FIXUP_LONG_LABELS;
        if(GetSystem()->FixupExpression(expr, errmsg, fixup_flags)) {
            // now we need to Explore() and set the dereferences on the expression to be word lookups
            auto cb = [&](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
                // skip everything that's not a DereferenceOp
                auto deref = dynamic_pointer_cast<BaseExpressionNodes::DereferenceOp>(node);
                if(!deref) return true;

                // create the word dereference 
                auto deref_func = [&](s64 in, s64* out, string& errmsg)->bool {
                    auto memory_view = GetMySystemInstance()->GetMemoryView();
                    if(!memory_view) {
                        errmsg = "Internal error";
                        return false;
                    }

                    *out = (u16)memory_view->Peek(in) | ((u16)memory_view->Peek(in + 1) << 8);
                    return true;
                };

                // set the function
                deref->SetDereferenceFunction(deref_func);
                return true;
            };

            if(!expr->Explore(cb, nullptr)) {
                errmsg = "Error in Explore()";
            } else {
                // valid, so evaluate the expression now to figure out our destination address
                s64 result;
                if(expr->Evaluate(&result, errmsg)) {
                    // truncate addresses in CPU and PPU modes
                    if(memory_mode < 2) result &= 0xFFFF;
                    // in bankable modes, convert to linear address
                    if(memory_mode == 2) {
                        result = ((result & 0xFF0000) >> 2) | (result & 0x3FFF);
                    } else if(memory_mode == 3) {
                        // TODO when labels are usable in CHR-ROM
                    }

                    // TODO properly check range on result
                    if(result < 0) {
                        errmsg = "address out of range";
                    } else {
                        // everything was a success!
                        set_address = false;
                        go_to_address = result;
                    }
                }
            }
        }
    }

    // if we get here and set_address is still true, then the expression failed to evaluate, so set the error
    // message and show the OKPopup
    if(set_address) {
        stringstream ss;
        ss << "There was a problem with the expression: " << errmsg;
        if(errloc >= 0) ss << " (at offset " << errloc << ")";
        address_error = ss.str();
        wait_dialog = true;
        set_address = false;
    }
}

void Memory::RenderTileDisplay(GlobalMemoryLocation const& start_address)
{
    auto si = GetMySystemInstance();
    auto system_view = si->GetMemoryViewAs<Systems::NES::SystemView>();
    if(!system_view) return;

    auto ppu_view = system_view->GetPPUView();
    if(!ppu_view) return;

    auto cartridge = GetSystem()->GetCartridge();
    if(!cartridge) return;

    if(!valid_texture) {
        GLuint gl_texture;
        glGenTextures(1, &gl_texture);
        glBindTexture(GL_TEXTURE_2D, gl_texture);
        // OpenGL requires at least one glTexImage2D to setup the texture
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        tile_display_texture = (void*)(intptr_t)gl_texture;
        glBindTexture(GL_TEXTURE_2D, 0);
        valid_texture = true;
    }

    GlobalMemoryLocation address = start_address;
    bool next_invalid = false;
    for(int tile_y = 0; tile_y < 32; tile_y++) {
        for(int tile_x = 0; tile_x < 32; tile_x++) {

            // read 16 bytes at `address`
            u8 tile_data[16];
            int tdi = 0;
            for(; tdi < 16 && !next_invalid; tdi++) {
                switch(memory_mode) {
                case 0: // CPU
                    tile_data[tdi] = system_view->Peek(address.address);
                    address.address += 1;
                    if(address.address == 0) next_invalid = true;
                    break;

                case 1: // PPU
                    tile_data[tdi] = ppu_view->PeekPPU(address.address);
                    address.address += 1;
                    if(address.address == 0) next_invalid = true;
                    break;

                case 2: // PRG-ROM
                    tile_data[tdi] = cartridge->ReadProgramRomRelative(address.prg_rom_bank, address.address);
                    address.address += 1;
                    if(address.address == 0x4000) {
                        address.address = 0;
                        address.prg_rom_bank += 1;
                        if(address.prg_rom_bank >= cartridge->header.num_prg_rom_banks) {
                            next_invalid = true;
                        }
                    }
                    break;

                case 3: // CHR-ROM
                    tile_data[tdi] = cartridge->ReadCharacterRomRelative(address.chr_rom_bank, address.address);
                    address.address += 1;
                    if(address.address == 0x2000) {
                        address.address = 0;
                        address.chr_rom_bank += 1;
                        if(address.chr_rom_bank >= cartridge->header.num_chr_rom_banks) {
                            next_invalid = true;
                        }
                    }
                    break;
                }
            }

            // if we didn't get enough data for a tile, clear out the image
            bool valid = (tdi == 16);

            // render 8x8 tile in tile_data
            for(int i = 0; i < 8; i++) {
                int y = tile_y * 8 + i;
                int byte0 = tile_data[i];
                int byte1 = tile_data[i+8];
                for(int j = 0; j < 8; j++) {
                    int x = tile_x * 8 + j;

                    if(valid) {
                        int bit0 = (byte0 >> (7-j)) & 0x01;
                        int bit1 = (byte1 >> (7-j)) & 0x01;
                        int color = (bit1 << 1) | bit0;
                        tile_display_framebuffer[y * 256 + x] = 0xFF000000 | (0x404040 * color);
                    } else {
                        tile_display_framebuffer[y * 256 + x] = 0xFF000000;
                    }
                }
            }
        }
    }

    // update the GL texture
    GLuint gl_texture = (GLuint)(intptr_t)tile_display_texture;
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, tile_display_framebuffer);
    glBindTexture(GL_TEXTURE_2D, 0);

    auto size = ImGui::GetWindowSize();
    float sz = min(size.x, size.y);
    ImGui::Image(tile_display_texture, ImVec2(sz, sz));
}

bool Memory::SaveWindow(std::ostream& os, std::string& errmsg)
{
    WriteVarInt(os, memory_mode);
    WriteVarInt(os, memory_size);
    WriteVarInt(os, memory_shift);
    WriteVarInt(os, selected_address);
    WriteString(os, address_text);
    WriteVarInt(os, (int)show_tile_display);
    if(!os.good()) {
        errmsg = "Error saving " + WindowPrefix();
        return false;
    }
    return true;
}

bool Memory::LoadWindow(std::istream& is, std::string& errmsg)
{
    memory_mode = ReadVarInt<int>(is);
    memory_size = ReadVarInt<int>(is);
    memory_shift = ReadVarInt<int>(is);
    selected_address = ReadVarInt<int>(is);
    ReadString(is, address_text);
    show_tile_display = (bool)ReadVarInt<int>(is);
    if(!is.good()) {
        errmsg = "Error loading " + WindowPrefix();
        return false;
    }
    return true;
}


} // namespace Windows::NES

