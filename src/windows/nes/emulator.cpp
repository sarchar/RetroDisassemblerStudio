#include <functional>
#include <memory>
#include <thread>

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

    ImGui::Separator();

    u64 next_uc = cpu->GetNextUC();
    // stop the system clock on invalid opcodes
    if(next_uc == (u64)-1) {
        current_state = State::PAUSED;
        ImGui::Text("Invalid opcode $%02X at $%04X", cpu->GetOpcode(), cpu->GetOpcodePC()-1);
    } else {
        string inst = disassembler->GetInstruction(cpu->GetOpcode());
        auto pc = cpu->GetOpcodePC();
        u8 operands[] = { memory_view->Read(pc), memory_view->Read(pc+1) };
        string operand = disassembler->FormatOperand(cpu->GetOpcode(), operands);
        ImGui::Text("Current inst: %s %s", inst.c_str(), operand.c_str());
        ImGui::Text("Next uc: 0x%X", next_uc);
    }

    ImGui::Separator();

    ImGui::Text("PC:$%04X", cpu->GetPC());
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

