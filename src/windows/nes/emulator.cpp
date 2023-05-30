#include <chrono>
#include <functional>
#include <memory>
#include <thread>

#include <GL/gl3w.h>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "magic_enum.hpp"

#include "main.h"
#include "util.h"

#include "systems/nes/nes_apu_io.h"
#include "systems/nes/nes_cpu.h"
#include "systems/nes/nes_disasm.h"
#include "systems/nes/nes_project.h"
#include "systems/nes/nes_ppu.h"
#include "systems/nes/nes_system.h"

#include "windows/nes/emulator.h"
#include "windows/nes/defines.h"
#include "windows/nes/labels.h"
#include "windows/nes/listing.h"
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

    SetIsDockSpace(true);

    *child_window_added += std::bind(&SystemInstance::ChildWindowAdded, this, placeholders::_1);

//!    // allocate storage for framebuffers
//!    framebuffer = (u32*)new u8[4 * 256 * 256];
//!    ram_framebuffer = (u32*)new u8[4 * 256 * 256];
//!    nametable_framebuffer = (u32*)new u8[4 * 256 * 256];
//!
//!    // fill the framebuffer with fully transparent pixels (0), so the bottom 16 rows aren't visible
//!    memset(framebuffer, 0, 4 * 256 * 256);
//!
//!    // generate the textures
//!    GLuint gl_texture;
//!
//!    glGenTextures(1, &gl_texture);
//!    glBindTexture(GL_TEXTURE_2D, gl_texture);
//!    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, framebuffer);
//!    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//!    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//!    framebuffer_texture = (void*)(intptr_t)gl_texture;
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
//!
//!    glBindTexture(GL_TEXTURE_2D, 0);
//!
   if(auto system = GetSystem()) {
       current_system = system;
//!
//!        auto& mv = memory_view;
//!        ppu = make_shared<PPU>([this]() {
//!                cpu->Nmi();
//!            },
//!
//!            // capturing the reference means the pointer can change after this initialization
//!            [this, &mv](u16 address)->u8 {
//!                return memory_view->ReadPPU(address);
//!            },
//!            [this, &mv](u16 address, u8 value)->void {
//!                memory_view->WritePPU(address, value);
//!            }
//!        );
//!
//!        apu_io = make_shared<APU_IO>();
//!        oam_dma_callback_connection = apu_io->oam_dma_callback->connect(std::bind(&System::WriteOAMDMA, this, placeholders::_1));
//!
//!        memory_view = system->CreateMemoryView(ppu->CreateMemoryView(), apu_io->CreateMemoryView());
//!
//!        cpu = make_shared<CPU>(
//!            std::bind(&MemoryView::Read, memory_view, placeholders::_1),
//!            std::bind(&MemoryView::Write, memory_view, placeholders::_1, placeholders::_2)
//!        );
//!
//!
//!        // start the emulation thread
//!        emulation_thread = make_shared<thread>(std::bind(&System::EmulationThread, this));
//!
       current_state = State::PAUSED;
   }

   Reset();
}

SystemInstance::~SystemInstance()
{
//!	exit_thread = true;
//!    if(emulation_thread) emulation_thread->join();
//!
//!    GLuint gl_texture = (GLuint)(intptr_t)framebuffer_texture;
//!    glDeleteTextures(1, &gl_texture);
//!
//!    gl_texture = (GLuint)(intptr_t)ram_texture;
//!    glDeleteTextures(1, &gl_texture);
//!
//!    gl_texture = (GLuint)(intptr_t)nametable_texture;
//!    glDeleteTextures(1, &gl_texture);
//!
//!    delete [] (u8*)framebuffer;
//!    delete [] (u8*)ram_framebuffer;
//!    delete [] (u8*)nametable_framebuffer;
}

void SystemInstance::CreateDefaultWorkspace()
{
    CreateNewWindow("Labels");
    CreateNewWindow("Defines");
    CreateNewWindow("Regions");
    CreateNewWindow("Listing");
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
    system_title = ss.str();
    SetTitle(system_title.c_str());
}

void SystemInstance::Update(double deltaTime)
{
    UpdateTitle();

//!    if(thread_exited) {
//!        cout << "uh oh thread exited" << endl;
//!    }
//!
//!    u64 cycle_count = cpu->GetCycleCount();
//!    auto current_time = chrono::steady_clock::now();
//!    u64 delta = cycle_count - last_cycle_count;
//!    double delta_time = (current_time - last_cycle_time) / 1.0s;
//!    if(delta_time >= 1.0) {
//!        cycles_per_sec = delta / delta_time;
//!        last_cycle_time = current_time;
//!        last_cycle_count = cycle_count;
//!    }
//!
//!    UpdateRAMTexture();
//!    UpdatePPUTexture();
//!    UpdateNametableTexture();
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

void SystemInstance::UpdatePPUTexture()
{
    GLuint gl_texture = (GLuint)(intptr_t)framebuffer_texture;
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, framebuffer);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SystemInstance::UpdateNametableTexture()
{
    int cx = 0;
    int cy = 0;
    int sz = 5;

    for(int y = 0; y < 30; y++) {
        for(int x = 0; x < 32; x++) {
            u8 t = memory_view->ReadPPU(0x2000 + y * 32 + x);

            // render 8x8
            for(int i = 0; i < 8; i++) {
                int cy = y * 8 + i;
                for(int j = 0; j < 8; j++) {
                    int cx = x * 8 + j;
                    // cycle between R/G/B colors
                    nametable_framebuffer[cy * 256 + cx] = 0xFF000000 | ((0x01 << ((t % 3) * 8)) * (u32)t);
                }
            }

        }
    }

    // update the opengl texture
    GLuint gl_texture = (GLuint)(intptr_t)nametable_texture;
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, nametable_framebuffer);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SystemInstance::Render()
{
    auto system = current_system.lock();
    if(!system) return;

//!    auto disassembler = system->GetDisassembler();
//!
//!    auto size = ImGui::GetWindowSize();
//!    size.x /= 2;
//!    ImGui::PushItemWidth(size.x / 2);
//!    ImGui::BeginChild("CPU view", size, false, ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoNavInputs);
//!
//!    if(ImGui::Button("Step Cycle")) {
//!        if(current_state == State::PAUSED) {
//!            current_state = State::STEP_CYCLE;
//!        }
//!    }
//!
//!    ImGui::SameLine();
//!    if(ImGui::Button("Step Inst")) {
//!        if(current_state == State::PAUSED) {
//!            current_state = State::STEP_INSTRUCTION;
//!        }
//!    }
//!
//!    ImGui::SameLine();
//!    if(ImGui::Button("Run")) {
//!        if(current_state == State::PAUSED) {
//!            current_state = State::RUNNING;
//!        }
//!    }
//!
//!    ImGui::SameLine();
//!    if(ImGui::Button("Pause")) {
//!        if(current_state == State::RUNNING) {
//!            current_state = State::PAUSED;
//!        }
//!    }
//!
//!    ImGui::SameLine();
//!    if(ImGui::Button("Reset")) {
//!        if(current_state == State::PAUSED) {
//!            Reset();
//!        }
//!    }
//!
//!    ImGui::SameLine();
//!    if(ImGui::InputText("Run-to", &run_to_address_str, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
//!        stringstream ss;
//!        ss << hex << run_to_address_str;
//!        ss >> run_to_address;
//!        current_state = State::RUNNING;
//!    }
//!
//!    ImGui::Text("%s :: %f Hz", magic_enum::enum_name(current_state).data(), cycles_per_sec);
//!
//!    ImGui::Separator();
//!
//!    u64 next_uc = cpu->GetNextUC();
//!    // stop the system clock on invalid opcodes
//!    if(next_uc == (u64)-1) {
//!        current_state = State::PAUSED;
//!        ImGui::Text("Invalid opcode $%02X at $%04X", cpu->GetOpcode(), cpu->GetOpcodePC()-1);
//!    } else {
//!        string inst = disassembler->GetInstruction(cpu->GetOpcode());
//!        auto pc = cpu->GetOpcodePC();
//!        u8 operands[] = { memory_view->Read(pc+1), memory_view->Read(pc+2) };
//!        string operand = disassembler->FormatOperand(cpu->GetOpcode(), operands);
//!        ImGui::Text("$%04X: %s %s (istep %d, uc=0x%X)", pc, inst.c_str(), operand.c_str(), cpu->GetIStep(), next_uc);
//!    }
//!
//!    ImGui::Separator();
//!
//!    ImGui::Text("PC:$%04X", cpu->GetPC()); ImGui::SameLine();
//!    ImGui::Text("S:$%04X", cpu->GetS()); ImGui::SameLine();
//!    ImGui::Text("A:$%02X", cpu->GetA()); ImGui::SameLine();
//!    ImGui::Text("X:$%02X", cpu->GetX()); ImGui::SameLine();
//!    ImGui::Text("Y:$%02X", cpu->GetY());
//!
//!    u8 p = cpu->GetP();
//!    char flags[] = "P:nv-bdizc";
//!    if(p & CPU_FLAG_N) flags[2] = 'N';
//!    if(p & CPU_FLAG_V) flags[3] = 'V';
//!    if(p & CPU_FLAG_B) flags[5] = 'B';
//!    if(p & CPU_FLAG_D) flags[6] = 'D';
//!    if(p & CPU_FLAG_I) flags[7] = 'I';
//!    if(p & CPU_FLAG_Z) flags[8] = 'Z';
//!    if(p & CPU_FLAG_C) flags[9] = 'C';
//!    ImGui::Text("%s", flags);
//!
//!    ImGui::Text("RAM[$00]=$%02X RAM[$02]=$%02X RAM[$03]=$%02X", memory_view->Read(0x00), memory_view->Read(0x02), memory_view->Read(0x03));
//!
//!    {
//!    }
//!
//!    ImGui::EndChild();
//!
//!    ImGui::SameLine();
//!    ImGui::PushItemWidth(size.x / 2);
//!    ImGui::BeginChild("PPU view", size);
//!
//!    ImGui::Image(framebuffer_texture, ImVec2(512, 512));
//!
//!    ImGui::Image(ram_texture, ImVec2(256, 256));
//!    ImGui::SameLine();
//!    ImGui::Image(nametable_texture, ImVec2(256, 256));
//!
//!    ImGui::EndChild();
}

void SystemInstance::CheckInput()
{
//!    // update all buttons on the joypad every frame
//!    apu_io->SetJoy1Pressed(NES_BUTTON_UP    , ImGui::IsKeyDown(ImGuiKey_W));
//!    apu_io->SetJoy1Pressed(NES_BUTTON_DOWN  , ImGui::IsKeyDown(ImGuiKey_S));
//!    apu_io->SetJoy1Pressed(NES_BUTTON_LEFT  , ImGui::IsKeyDown(ImGuiKey_A));
//!    apu_io->SetJoy1Pressed(NES_BUTTON_RIGHT , ImGui::IsKeyDown(ImGuiKey_D));
//!    apu_io->SetJoy1Pressed(NES_BUTTON_SELECT, ImGui::IsKeyDown(ImGuiKey_Tab));
//!    apu_io->SetJoy1Pressed(NES_BUTTON_START , ImGui::IsKeyDown(ImGuiKey_Enter));
//!    apu_io->SetJoy1Pressed(NES_BUTTON_B     , ImGui::IsKeyDown(ImGuiKey_Period));
//!    apu_io->SetJoy1Pressed(NES_BUTTON_A     , ImGui::IsKeyDown(ImGuiKey_Slash));
}

void SystemInstance::Reset()
{
//!    cpu->Reset();
//!    ppu->Reset();
//!    cpu_shift = 0;
//!    raster_line = framebuffer;
//!    raster_y = 0;
//!    oam_dma_enabled = false;
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
            SingleCycle();
            current_state = State::PAUSED;
            break;

        case State::STEP_INSTRUCTION:
            while(current_state == State::STEP_INSTRUCTION && !SingleCycle()) ;
            if(current_state == State::STEP_INSTRUCTION) {
                current_state = State::PAUSED;
            }
            break;

        case State::RUNNING:
            while(!exit_thread && current_state == State::RUNNING) {
                SingleCycle();
                if(run_to_address == cpu->GetOpcodePC()) {
                    current_state = State::PAUSED;
                    break;
                }
            }
            break;

        case State::CRASHED:

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

} // namespace Windows::NES

