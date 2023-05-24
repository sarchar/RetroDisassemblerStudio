#include <functional>
#include <memory>
#include <thread>

#include "magic_enum.hpp"

#include "main.h"
#include "util.h"

#include "systems/nes/nes_disasm.h"
#include "systems/nes/nes_project.h"
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

    if(auto system = MyApp::Instance()->GetProject()->GetSystem<System>()) {
        current_system = system;

        memory_view = system->CreateMemoryView();

        cpu = make_shared<CPU>(
            std::bind(&MemoryView::Read, memory_view, placeholders::_1),
            std::bind(&MemoryView::Write, memory_view, placeholders::_1, placeholders::_2)
            //[](u16 address)->u8 {
            //    cout << "read(" << hex << setw(4) << setfill('0') << address << ") = $EA" << endl;
            //    return 0xEA;
            //},

            //[](u16 address, u8 value)->void {
            //    cout << "write(" << hex << setw(4) << setfill('0') << address << ", " << value << ")" << endl;
            //}
        );

        // start the emulation thread
        emulation_thread = make_shared<thread>(std::bind(&Emulator::EmulationThread, this));

        current_state = State::PAUSED;
    }
}

Emulator::~Emulator()
{
	exit_thread = true;
    if(emulation_thread) emulation_thread->join();
}

void Emulator::UpdateContent(double deltaTime)
{
    if(thread_exited) {
        cout << "uh oh thread exited" << endl;
    }
}

void Emulator::RenderContent()
{
    auto system = current_system.lock();
    if(!system) return;
    auto disassembler = system->GetDisassembler();

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
            cpu->Reset();
        }
    }

    ImGui::SameLine();
    ImGui::Text("%s", magic_enum::enum_name(current_state).data());

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
}

void Emulator::CheckInput()
{
}

void Emulator::EmulationThread()
{
    while(!exit_thread) {
        switch(current_state) {
        case State::INIT:
        case State::PAUSED:
            break;

        case State::STEP_CYCLE:
            cpu->Step();
            current_state = State::PAUSED;
            break;

        case State::STEP_INSTRUCTION:
            while(current_state == State::STEP_INSTRUCTION && !cpu->Step()) ;
            if(current_state == State::STEP_INSTRUCTION) {
                current_state = State::PAUSED;
            }
            break;

        case State::RUNNING:
            while(!exit_thread && current_state == State::RUNNING) {
                cpu->Step();
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

