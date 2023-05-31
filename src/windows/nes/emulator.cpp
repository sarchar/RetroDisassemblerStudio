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
#include "systems/nes/cpu.h"
#include "systems/nes/disasm.h"
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

    *child_window_added += std::bind(&SystemInstance::ChildWindowAdded, this, placeholders::_1);

    // allocate storage for framebuffers
    framebuffer = (u32*)new u8[4 * 256 * 256];
    ram_framebuffer = (u32*)new u8[4 * 256 * 256];
    nametable_framebuffer = (u32*)new u8[4 * 256 * 256];

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

    glBindTexture(GL_TEXTURE_2D, 0);

    if(current_system = GetSystem()) {
        auto& mv = memory_view;

        ppu = make_shared<PPU>(
            [this]() {
                cpu->Nmi();
            },
            [this, &mv](u16 address)->u8 { // capturing the reference means the pointer can change after this initialization
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
            std::bind(&MemoryView::Read, memory_view, placeholders::_1),
            std::bind(&MemoryView::Write, memory_view, placeholders::_1, placeholders::_2)
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
//!    delete [] (u8*)nametable_framebuffer;
}

void SystemInstance::CreateDefaultWorkspace()
{
    CreateNewWindow("Labels");
    CreateNewWindow("Defines");
    CreateNewWindow("Regions");
    CreateNewWindow("Listing");
    CreateNewWindow("Screen");
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

    // check for global keystrokes that should work in all windows
    bool is_current_instance = (GetSystemInstance().get() == this);
    if(is_current_instance) {
        if(ImGui::IsKeyPressed(ImGuiKey_F5)) {
            if(current_state == State::PAUSED) {
                current_state = State::RUNNING;
            }
        }

        if(ImGui::IsKeyPressed(ImGuiKey_Escape) && ImGui::IsKeyPressed(ImGuiKey_LeftCtrl)) {
            cout << GetTitle() << " got ESCAPE" << endl;
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
//!    ImGui::SameLine();
//!    if(ImGui::InputText("Run-to", &run_to_address_str, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
//!        stringstream ss;
//!        ss << hex << run_to_address_str;
//!        ss >> run_to_address;
//!        current_state = State::RUNNING;
//!    }
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

            running = false;
            break;

        case State::RUNNING:
            running = true;
            while(!exit_thread && current_state == State::RUNNING) {
                SingleCycle();
                if(run_to_address == cpu->GetOpcodePC()) {
                    current_state = State::PAUSED;
                    break;
                }
            }
            running = false;
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
}

PPUState::~PPUState()
{
}

void PPUState::CheckInput()
{
}

void PPUState::Update(double deltaTime)
{
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
    ImGui::Text("Nametables TODO");
}

void PPUState::RenderPalettes(std::shared_ptr<PPU> const& ppu)
{
    ImGui::Text("Palettes TODO");
}

void PPUState::RenderSprites(std::shared_ptr<PPU> const& ppu)
{
    ImGui::Text("Sprites TODO");
}

} // namespace Windows::NES

