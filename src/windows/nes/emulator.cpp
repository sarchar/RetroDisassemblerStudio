#include <chrono>
#include <functional>
#include <memory>
#include <thread>

#include <GL/gl3w.h>

#include "magic_enum.hpp"

#include "main.h"
#include "util.h"

#include "systems/nes/nes_disasm.h"
#include "systems/nes/nes_project.h"
#include "systems/nes/nes_ppu.h"
#include "systems/nes/nes_system.h"
#include "windows/nes/emulator.h"

using namespace std;

namespace NES {
namespace Windows {

std::shared_ptr<Emulator> Emulator::CreateWindow()
{
    return make_shared<Emulator>();
}

Emulator::Emulator()
    : BaseWindow("NES::Emulator")
{
    SetTitle("Emulator :: Paused");

    framebuffer = (u32*)new u8[4 * 256 * 256];
    for(u32 i = 0; i < 256 * 256; i++) {
        framebuffer[i] = 0xFF000000; // 0xAARRGGBB
    }

    GLuint _display_texture;
    glGenTextures(1, &_display_texture);
    glBindTexture(GL_TEXTURE_2D, _display_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, framebuffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    display_texture = (void*)(intptr_t)_display_texture;

    if(auto system = MyApp::Instance()->GetProject()->GetSystem<System>()) {
        current_system = system;

        ppu = make_shared<PPU>([this]() {
            cpu->Nmi();
        });

        memory_view = system->CreateMemoryView(ppu->CreateMemoryView());

        cpu = make_shared<CPU>(
            std::bind(&MemoryView::Read, memory_view, placeholders::_1),
            std::bind(&MemoryView::Write, memory_view, placeholders::_1, placeholders::_2)
        );


        // start the emulation thread
        emulation_thread = make_shared<thread>(std::bind(&Emulator::EmulationThread, this));

        current_state = State::PAUSED;
    }

    Reset();
}

Emulator::~Emulator()
{
	exit_thread = true;
    if(emulation_thread) emulation_thread->join();

    GLuint _display_texture = (GLuint)(intptr_t)display_texture;
    glDeleteTextures(1, &_display_texture);

    delete [] (u8*)framebuffer;
}

void Emulator::UpdateContent(double deltaTime)
{
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

    static int c = 0;
    int color = 0xFF000000 | c;
    c++;
    int cx = 0;
    int cy = 0;
    int sz = 5;
    for(int i = 0; i < 0x800; i++) {
        u8 v = memory_view->Read(i);
        // render 8x8
        for(int i = 0; i < sz; i++) {
            int y = cy + i;
            for(int j = 0; j < sz; j++) {
                int x = cx + j;
                framebuffer[y * 256 + x] = 0xFF000000 | (0x010101 * (u32)v);
            }
        }
        cx += sz;
        if(cx >= 256) {
            cx = 0;
            cy += sz;
        }
    }
}

void Emulator::RenderContent()
{
    auto system = current_system.lock();
    if(!system) return;
    auto disassembler = system->GetDisassembler();

    auto size = ImGui::GetWindowSize();
    size.x /= 2;
    ImGui::PushItemWidth(size.x / 2);
    ImGui::BeginChild("CPU view", size);

    if(ImGui::Button("Step Cycle")) {
        if(current_state == State::PAUSED) {
            current_state = State::STEP_CYCLE;
        }
    }

    ImGui::SameLine();
    if(ImGui::Button("Step Inst")) {
        if(current_state == State::PAUSED) {
            current_state = State::STEP_INSTRUCTION;
        }
    }

    ImGui::SameLine();
    if(ImGui::Button("Run")) {
        if(current_state == State::PAUSED) {
            current_state = State::RUNNING;
        }
    }

    ImGui::SameLine();
    if(ImGui::Button("Pause")) {
        if(current_state == State::RUNNING) {
            current_state = State::PAUSED;
        }
    }

    ImGui::SameLine();
    if(ImGui::Button("Reset")) {
        if(current_state == State::PAUSED) {
            Reset();
        }
    }

    ImGui::Text("%s :: %f Hz", magic_enum::enum_name(current_state).data(), cycles_per_sec);

    ImGui::Separator();

    u64 next_uc = cpu->GetNextUC();
    // stop the system clock on invalid opcodes
    if(next_uc == (u64)-1) {
        current_state = State::PAUSED;
        ImGui::Text("Invalid opcode $%02X at $%04X", cpu->GetOpcode(), cpu->GetOpcodePC()-1);
    } else {
        string inst = disassembler->GetInstruction(cpu->GetOpcode());
        auto pc = cpu->GetOpcodePC();
        u8 operands[] = { memory_view->Read(pc+1), memory_view->Read(pc+2) };
        string operand = disassembler->FormatOperand(cpu->GetOpcode(), operands);
        ImGui::Text("$%04X: %s %s (istep %d, uc=0x%X)", pc, inst.c_str(), operand.c_str(), cpu->GetIStep(), next_uc);
    }

    ImGui::Separator();

    ImGui::Text("PC:$%04X", cpu->GetPC()); ImGui::SameLine();
    ImGui::Text("S:$%04X", cpu->GetS()); ImGui::SameLine();
    ImGui::Text("A:$%02X", cpu->GetA()); ImGui::SameLine();
    ImGui::Text("X:$%02X", cpu->GetX()); ImGui::SameLine();
    ImGui::Text("Y:$%02X", cpu->GetY());

    u8 p = cpu->GetP();
    char flags[] = "P:nvb-dizc";
    if(p & CPU_FLAG_N) flags[2] = 'N';
    if(p & CPU_FLAG_V) flags[3] = 'V';
    if(p & CPU_FLAG_B) flags[4] = 'B';
    if(p & CPU_FLAG_D) flags[6] = 'D';
    if(p & CPU_FLAG_I) flags[7] = 'I';
    if(p & CPU_FLAG_Z) flags[8] = 'Z';
    if(p & CPU_FLAG_C) flags[9] = 'C';
    ImGui::Text("%s", flags);

    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::PushItemWidth(size.x / 2);
    ImGui::BeginChild("PPU view", size);

    // update the opengl texture
    GLuint _display_texture = (GLuint)(intptr_t)display_texture;
    glBindTexture(GL_TEXTURE_2D, _display_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, framebuffer);
    glBindTexture(GL_TEXTURE_2D, 0);

    ImGui::Image(display_texture, ImVec2(256, 256));

    ImGui::EndChild();
}

void Emulator::CheckInput()
{
}

void Emulator::Reset()
{
    cpu->Reset();
    ppu->Reset();
}

bool Emulator::SingleCycle()
{
    bool ret = cpu->Step();

    // PPU clock is /4 master clock and CPU is /12 master clock
    ppu->Step();
    ppu->Step();
    ppu->Step();

    return ret;
}

void Emulator::EmulationThread()
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

}
}

