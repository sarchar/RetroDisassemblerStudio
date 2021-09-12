#include <algorithm>
#include <cassert>

#include "systems/snes/cpu65c816.h"
#include "systems/snes/snes_system.h"

using namespace std;

SNESSystem::SNESSystem()
{
    BuildSystemComponents();  // build the entire system
    CreateSystemThread();     // create the thread (and run it) but wait for start signal
}

SNESSystem::~SNESSystem()
{
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

    // create the cpu
    cpu = make_unique<CPU65C816>();

    // connect CPU to the system
    cpu->pins.phi2.Connect(&system_clock->pins.out);

    reset_wire.AssertHigh();
    cpu->pins.reset_n.Connect(&reset_wire);

    //system_ram = make_unique<SNESRam>();
    //cpu.pins.address_bus.connect(system_ram.pins.address_bus)
}

void SNESSystem::CreateSystemThread()
{
    system_thread = make_unique<std::thread>(std::bind(&SNESSystem::SystemThreadMain, this));
    for(int i = 0; i < 10000000; i++) {  }
    cond_start_clock.notify_all();
}

void SNESSystem::SystemThreadMain()
{
    cout << "thread started" << endl;

    // I'm not knowledgable enough with the STL to avoid the mutex with
    // the wait event/condition variable, but the mutex isn't needed
    // and ideally the lock wouldn't be needed either. I want cond_start_clock.wait()
    std::mutex mut;
    std::unique_lock<std::mutex> lock(mut);

    while(true) {
        cond_start_clock.wait(lock);
        lock.unlock();

        // TEMP run the clock some
        for(int i = 0; i < 10; i++) {
            system_clock->Step();
        }

        break;
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

