#include <algorithm>
#include <cassert>

#include "clock_divider.h"
#include "systems/snes/cpu65c816.h"
#include "systems/snes/snes_system.h"

using namespace std;

SNESSystem::SNESSystem()
{
    BuildSystemComponents();  // build the entire system
    CreateSystemThread();     // create the thread (and run it) but wait for start signal
    IssueReset();             // reset the CPU
}

SNESSystem::~SNESSystem()
{
    IssueExitThread();
    system_thread->join();
    cout << "joined" << endl;
}

#include "wires.h"
void SNESSystem::BuildSystemComponents()
{

    // the system clock needs to be 180 out of phase (idle = high, as the falling edge starts a clock cycle)
    system_clock = make_unique<SystemClock>(SNES_CLOCK_FREQUENCY);
    system_clock->Enable();
    system_clock->StepToHigh();

    // the CPU clock gets divided from the master clock
    // watch for the falling edge, and start the clock high
    cpu_clock = make_unique<ClockDivider>(SNES_CPU_CLOCK_DIVIDER, 0, 1);
    cpu_clock->pins.in.Connect(&system_clock->pins.out);

    // create the cpu
    cpu = make_unique<CPU65C816>();

    // connect CPU to the system
    cpu->pins.phi2.Connect(&cpu_clock->pins.out); // connect the clock

    reset_wire.AssertHigh();  // default reset to high when everything is attached
    cpu->pins.reset_n.Connect(&reset_wire);

    // TEMP let's monitor some wires
    *cpu->pins.e.signal_changed    += [](Wire*, tristate new_state) { cout << "E   = " << (int)new_state << endl; };
    *cpu->pins.vda.signal_changed  += [](Wire*, tristate new_state) { cout << "VDA = " << (int)new_state << endl; };
    *cpu->pins.vpa.signal_changed  += [](Wire*, tristate new_state) { cout << "VPA = " << (int)new_state << endl; };
    *cpu->pins.mx.signal_changed   += [](Wire*, tristate new_state) { cout << "MX  = " << (int)new_state << endl; };
    *cpu->pins.rw_n.signal_changed += [](Wire*, tristate new_state) { cout << "RWn = " << (int)new_state << endl; };
    *cpu->pins.vp_n.signal_changed += [](Wire*, tristate new_state) { cout << "VPn = " << (int)new_state << endl; };

    //system_ram = make_unique<SNESRam>();
    //cpu.pins.address_bus.connect(system_ram.pins.address_bus)
}

void SNESSystem::CreateSystemThread()
{
    system_thread = make_unique<std::thread>(std::bind(&SNESSystem::SystemThreadMain, this));
}

void SNESSystem::IssueReset()
{
    // must be waiting for a command
    std::mutex mut;
    std::unique_lock<std::mutex> lock(mut);
    system_thread_command_done_condition.wait(lock);
    lock.unlock();

    system_thread_command = CMD_RESET;
    system_thread_command_start_condition.notify_one();
}

void SNESSystem::IssueExitThread()
{
    // must be waiting for a command
    std::mutex mut;
    std::unique_lock<std::mutex> lock(mut);
    system_thread_command_done_condition.wait(lock);
    lock.unlock();

    system_thread_command = CMD_EXIT_THREAD;
    system_thread_command_start_condition.notify_one();
}

void SNESSystem::SystemThreadMain()
{
    bool running = true;

    cout << "system thread started" << endl;

    // I'm not knowledgable enough with the STL to avoid the mutex with
    // the wait event/condition variable, but the mutex isn't needed
    // and ideally the lock wouldn't be needed either. 
    // I want system_thread_command_start_condition.wait()
    std::mutex mut;
    std::unique_lock<std::mutex> lock(mut);

    // start by telling the main thread that we're ready and can accept the next command
    system_thread_command_done_condition.notify_one();

    for(;running;) {
        system_thread_command_start_condition.wait(lock);
        lock.unlock();

        switch(system_thread_command) {
        case CMD_RESET:
            // assert reset low, step the clock, then assert reset high
            reset_wire.AssertLow();
            for(int i = 0; i < 1 * SNES_CPU_CLOCK_DIVIDER; i++) {
                system_clock->Step();
            }
            reset_wire.AssertHigh();
            break;

        case CMD_EXIT_THREAD:
            running = false;
            break;
        }

        system_thread_command_done_condition.notify_one();
    }
}

bool SNESSystem::LoadROM(string const& file_path_name)
{
    return false;
}

bool SNESSystem::IsROMValid(std::string const& file_path_name, std::istream& is)
{
    // TODO this is duplicated, make it a utility function somewhere
    auto ends_with = [](std::string const& value, std::string const& ending) {
        if (ending.size() > value.size()) return false;
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    };

    string lcase_file_path_name = file_path_name;
    std::transform(lcase_file_path_name.begin(), lcase_file_path_name.end(), 
                   lcase_file_path_name.begin(), [](unsigned char c){ return std::tolower(c); });

    if(ends_with(lcase_file_path_name, ".bin")) {
        return true;
    } else if(ends_with(lcase_file_path_name, ".smc")) {
        assert(false); // TODO
    }

    return false;
}

System::Information const* SNESSystem::GetInformation()
{
    return SNESSystem::GetInformationStatic();
}

System::Information const* SNESSystem::GetInformationStatic()
{
    static System::Information information = {
        .abbreviation = "SNES",
        .full_name = "Super Nintendo Entertainment System",
        .is_rom_valid = std::bind(&SNESSystem::IsROMValid, placeholders::_1, placeholders::_2),
        .create_system = std::bind(&SNESSystem::CreateSystem)
    };
    return &information;
}

shared_ptr<System> SNESSystem::CreateSystem()
{
    SNESSystem* snes_system = new SNESSystem();
    return shared_ptr<System>(snes_system);
}

