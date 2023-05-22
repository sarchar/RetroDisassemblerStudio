#include <functional>
#include <memory>
#include <thread>

#include "main.h"
#include "util.h"

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

        current_state = State::RUNNING;
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
    ImGui::Text("emulator goes here lul");
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

