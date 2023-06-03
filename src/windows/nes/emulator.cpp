#include <bitset>
#include <chrono>
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

#include "windows/nes/emulator.h"
#include "windows/nes/defines.h"
#include "windows/nes/labels.h"
#include "windows/nes/listing.h"
#include "windows/nes/project.h"
#include "windows/nes/regions.h"

using namespace std;

namespace Windows::NES {

int SystemInstance::next_system_id = 1;

std::shared_ptr<SystemInstance> SystemInstance::CreateWindow()
{
    return make_shared<SystemInstance>();
}

SystemInstance::SystemInstance()
    : BaseWindow("NES::SystemInstance")
{
    system_id = next_system_id++;
    SetNav(false);

    SetShowMenuBar(true);
    SetIsDockSpace(true);

    breakpoint_hit = make_shared<breakpoint_hit_t>();
    *child_window_added += std::bind(&SystemInstance::ChildWindowAdded, this, placeholders::_1);
    
    // allocate cpu_quick_breakpoints
    auto size = 0x10000 / (8 * sizeof(u32)); // one bit for 64KiB memory space
    cout << WindowPrefix() << "allocated " << dec << size << " bytes for CPU breakpoint cache" << endl;
    cpu_quick_breakpoints = new u32[size];
    memset(cpu_quick_breakpoints, 0, size);

    // allocate storage for framebuffers
    framebuffer = (u32*)new u8[4 * 256 * 256];
    ram_framebuffer = (u32*)new u8[4 * 256 * 256];

    // fill the framebuffer with fully transparent pixels (0), so the bottom 16 rows aren't visible
    memset(framebuffer, 0, 4 * 256 * 256);

    // generate the textures
//!    GLuint gl_texture;
//!
//!    glGenTextures(1, &gl_texture);
//!    glBindTexture(GL_TEXTURE_2D, gl_texture);
//!    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, ram_framebuffer);
//!    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//!    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//!    ram_texture = (void*)(intptr_t)gl_texture;
//!
//!    glGenTextures(1, &gl_texture);
//!    glBindTexture(GL_TEXTURE_2D, gl_texture);
//!    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nametable_framebuffer);
//!    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//!    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//!    nametable_texture = (void*)(intptr_t)gl_texture;

//!    glBindTexture(GL_TEXTURE_2D, 0);

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

   Reset();
}

SystemInstance::~SystemInstance()
{
    exit_thread = true;
    if(emulation_thread) emulation_thread->join();

//!    GLuint gl_texture = (GLuint)(intptr_t)framebuffer_texture;
//!    gl_texture = (GLuint)(intptr_t)ram_texture;
//!    glDeleteTextures(1, &gl_texture);
//!
//!    gl_texture = (GLuint)(intptr_t)nametable_texture;
//!    glDeleteTextures(1, &gl_texture);

    delete [] (u8*)framebuffer;
//!    delete [] (u8*)ram_framebuffer;

    delete [] cpu_quick_breakpoints;
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
    bpi->enabled = true;
    bpi->break_execute = true;
    SetBreakpoint(where, bpi);

    CreateNewWindow("Labels");
    CreateNewWindow("Defines");
    CreateNewWindow("Regions");
    CreateNewWindow("Listing");
    CreateNewWindow("Screen");
    CreateNewWindow("Breakpoints");
    CreateNewWindow("Watch");
    CreateNewWindow("PPUState");
    CreateNewWindow("CPUState");
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
    } else if(window_type == "Screen") {
        wnd = Screen::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_TOPRIGHT);
    } else if(window_type == "CPUState") {
        wnd = CPUState::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_BOTTOMRIGHT);
    } else if(window_type == "PPUState") {
        wnd = PPUState::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_BOTTOMRIGHT);
    } else if(window_type == "Watch") {
        wnd = Watch::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_BOTTOMRIGHT);
    } else if(window_type == "Breakpoints") {
        wnd = Breakpoints::CreateWindow();
        wnd->SetInitialDock(BaseWindow::DOCK_BOTTOMRIGHT);
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
    ss << "NES_" << system_id << " :: " << magic_enum::enum_name(current_state);
    // append ImGui fixed ID, so as to ignore our title text
    ss << "###NES_" << system_id;
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

//!    UpdateRAMTexture();
}

void SystemInstance::UpdateRAMTexture()
{
    int cx = 0;
    int cy = 0;
    int sz = 5;

    for(int i = 0; i < 0x800; i++) { // iterate over ram
        u8 v = memory_view->Read(i);

        // render sz x sz square of the ram value
        for(int i = 0; i < sz; i++) {
            int y = cy + i;
            for(int j = 0; j < sz; j++) {
                int x = cx + j;
                // cycle between R/G/B colors
                ram_framebuffer[y * 256 + x] = 0xFF000000 | ((0x01 << ((v % 3) * 8)) * (u32)v);
            }
        }

        // next square
        cx += sz;
        if(cx >= 256) {
            cx = 0;
            cy += sz;
        }
    }

    // update the opengl texture
    GLuint gl_texture = (GLuint)(intptr_t)ram_texture;
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, ram_framebuffer);
    glBindTexture(GL_TEXTURE_2D, 0);
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

void SystemInstance::Render()
{
//!    ImGui::Image(ram_texture, ImVec2(256, 256));
//!    ImGui::SameLine();
//!    ImGui::Image(nametable_texture, ImVec2(256, 256));
}

void SystemInstance::CheckInput()
{
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

    auto system_view = dynamic_pointer_cast<Systems::NES::SystemView>(memory_view);
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
        // move scanline down
        raster_line = &framebuffer[++raster_y * 256];
        hblank = hblank_new;
    } else if(!hblank_new) {
        hblank = false;
        // display color
        *raster_line++ = (0xFF000000 | color);
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

void SystemInstance::CheckBreakpoints(u16 address, CheckBreakpointMode mode)
{
    GlobalMemoryLocation where = {
        .address      = address,
        .is_chr       = 0,
        .prg_rom_bank = 0,
    };

    [[likely]] if(where.address & 0x8000) { // determine bank when in bankable space
        auto system_view = dynamic_pointer_cast<Systems::NES::SystemView>(memory_view);
        where.prg_rom_bank = system_view->GetCartridgeView()->GetRomBank(where.address);
    }

    auto check_bp = [&](shared_ptr<BreakpointInfo> const& bp)->bool {
        auto break_execute = bp->break_execute && (mode == CheckBreakpointMode::EXECUTE);
        auto break_read    = bp->break_read    && (mode == CheckBreakpointMode::READ);
        auto break_write   = bp->break_write   && (mode == CheckBreakpointMode::WRITE);
        if(bp->enabled && (break_read || break_write || break_execute)) {
            current_state = State::PAUSED;
            breakpoint_hit->emit(bp);
            return true;
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

std::shared_ptr<Screen> Screen::CreateWindow()
{
    return make_shared<Screen>();
}

Screen::Screen()
    : BaseWindow("Windows::NES::Screen")
{
    SetNav(false);
    SetNoScrollbar(true);
    SetTitle("Screen");

    GLuint gl_texture;

    glGenTextures(1, &gl_texture);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    // OpenGL requires at least one glTexImage2D to setup the texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    framebuffer_texture = (void*)(intptr_t)gl_texture;
    glBindTexture(GL_TEXTURE_2D, 0);
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
    if(auto framebuffer = GetMySystemInstance()->GetFramebuffer()) {
        GLuint gl_texture = (GLuint)(intptr_t)framebuffer_texture;
        glBindTexture(GL_TEXTURE_2D, gl_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, framebuffer);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void Screen::PreRender()
{
    // won't really be necessary if the window starts docked
    ImGui::SetNextWindowSize(ImVec2(324, 324), ImGuiCond_Appearing);
}

void Screen::Render()
{
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
    : BaseWindow("Windows::NES::CPUState")
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
    : BaseWindow("Windows::NES::PPUState")
{
    SetTitle("PPU");

    // allocate storage for the nametable rendering
    nametable_framebuffer = (u32*)new u8[sizeof(int) * 512 * 512];
    memset(nametable_framebuffer, 0, sizeof(int) * 512 * 512);

    // generate the nametable GL texture
    GLuint gl_texture;

    glGenTextures(1, &gl_texture);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    nametable_texture = (void*)(intptr_t)gl_texture;

    glBindTexture(GL_TEXTURE_2D, 0);

}

PPUState::~PPUState()
{
    GLuint gl_texture = (GLuint)(intptr_t)nametable_texture;
    glDeleteTextures(1, &gl_texture);

    delete [] (u8*)nametable_framebuffer;
}

void PPUState::CheckInput()
{
}

void PPUState::Update(double deltaTime)
{
    if(display_mode == 1) {
        UpdateNametableTexture();
    }
}

void PPUState::Render()
{
    auto si = GetMySystemInstance();
    auto ppu = si->GetPPU();
    if(!ppu) return;

    auto memory_view = si->GetMemoryView();
    if(!memory_view) return;

    ImGui::RadioButton("Registers", &display_mode, 0); ImGui::SameLine();
    ImGui::RadioButton("Nametables", &display_mode, 1); ImGui::SameLine();
    ImGui::RadioButton("Palettes", &display_mode, 2); ImGui::SameLine();
    ImGui::RadioButton("Sprites", &display_mode, 3);

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
    ImGui::Text("Palettes TODO");
}

void PPUState::RenderSprites(std::shared_ptr<PPU> const& ppu)
{
    ImGui::Text("Sprites TODO");
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

std::shared_ptr<Watch> Watch::CreateWindow()
{
    return make_shared<Watch>();
}

Watch::Watch()
    : BaseWindow("Windows::NES::Watch")
{
    SetTitle("Watch");

    // TODO delete me someday
    CreateWatch("*$00");
    CreateWatch("*$01");
    CreateWatch("*$02");
    CreateWatch("*$03");
    CreateWatch("*$04");
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
            if(reverse_sort) diff = bp->expression_string <= ap->expression_string;
            else             diff = ap->expression_string <= bp->expression_string;
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
                if(ImGui::IsItemHovered()) {
                    if(ImGui::IsMouseDoubleClicked(0)) {
                        editing = row;
                        edit_string = watch_data->expression_string;
                        started_editing = true;
                    } else if(ImGui::IsMouseClicked(1)) {
                        selected_row = watch_index;
                        ImGui::OpenPopup("watch_context_menu");
                    }
                }

                ImGui::Text("%s", watch_data->expression_string.c_str());
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

            // expression was valid from a grammar point of view, now apply semantics
            // allow labels, defines, derefs, but not addressing modes
            if(GetSystem()->FixupExpression(expr, errmsg, true, true, true, false)) {
                // Expression contained valid elements, now DereferenceOp nodes need evaluation functions set
                if(SetDereferenceOp(watch_data)) {
                    // success, done editing and re-sort after adding
                    do_set_watch = false;
                    editing = -1;
                    need_resort = true;

                    // cache the expression string
                    watch_data->expression_string = edit_string;
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

std::shared_ptr<Breakpoints> Breakpoints::CreateWindow()
{
    return make_shared<Breakpoints>();
}

Breakpoints::Breakpoints()
    : BaseWindow("Windows::NES::Breakpoints")
{
    SetTitle("Breakpoints");

}

Breakpoints::~Breakpoints()
{
}

void Breakpoints::CheckInput()
{
}

void Breakpoints::Update(double deltaTime)
{
}

void Breakpoints::Render()
{
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

        int row = 0;
        GetMySystemInstance()->IterateBreakpoints([&](shared_ptr<BreakpointInfo> const& bpi) {
            ImGui::PushID(row);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // show selection even when editing
            ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
            if(ImGui::Selectable("##selectable", selected_row == row, selectable_flags)) selected_row = row;

            // when the user activates a breakpoint, go to it in the listing window
            if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                if(auto listing = GetMyListing()) {
                    listing->GoToAddress(bpi->address);
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

            ImGui::TableNextColumn();
            stringstream ss;
            bpi->address.FormatAddress(ss);
            ImGui::Text(ss.str().c_str());

            ImGui::TableNextColumn();
            ImGui::Text("cpu.X==3");

            ImGui::PopID();
            row++;
        });

        ImGui::EndTable();
    }

    ImGui::PopStyleVar(3);
}

} // namespace Windows::NES

