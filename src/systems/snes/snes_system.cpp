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

    // TEMP
    IssueStep();
    IssueStep();
    IssueStep();
}

SNESSystem::~SNESSystem()
{
    IssueExitThread();
    system_thread->join();
    cout << "[SNESSystem] system thread joined" << endl;
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

    // we'll need a clock delay so that data signals can be held long enough to be sampled
    cpu_clock_delay = make_unique<SignalDelay<bool>>(false, 1, SNES_CPU_CLOCK_DIVIDER/2); // on the falling edge, delay the signal by 1, and reset the counter every half cpu clock cycle
    cpu_clock_delay->pins.clk.Connect(&system_clock->pins.out);
    cpu_clock_delay->pins.in.Connect(&cpu_clock->pins.out);
    cpu_clock_delay->Transfer(); // transfer the dead state of the clock right now

    // and another clock delay for CPU signal line setup
    cpu_signal_setup_delay = make_unique<SignalDelay<bool>>(false, 2, SNES_CPU_CLOCK_DIVIDER/2); // on the falling edge, delay the signal by 2, and reset the counter every half cpu clock cycle
    cpu_signal_setup_delay->pins.clk.Connect(&system_clock->pins.out);
    cpu_signal_setup_delay->pins.in.Connect(&cpu_clock->pins.out);
    cpu_signal_setup_delay->Transfer(); // transfer the dead state of the clock right now

    // create the cpu
    cpu = make_unique<CPU65C816>();

    // connect CPU to the system
    cpu->pins.phi2.Connect(&cpu_clock->pins.out); // connect the clock
    cpu->pins.signal_setup.Connect(&cpu_signal_setup_delay->pins.out); // connect the signal setup lines

    reset_wire.AssertHigh();  // default reset to high when everything is attached
    cpu->pins.reset_n.Connect(&reset_wire);

    // create the address decoder
    address_decoder = make_unique<SNESAddressDecoder>();
    address_decoder->pins.vda.Connect(&cpu->pins.vda);
    address_decoder->pins.vpa.Connect(&cpu->pins.vpa);
    address_decoder->pins.a.Connect(&cpu->pins.a);

    // create the main ram
    main_ram = make_unique<RAM<u16, u8>>(16, 1); // 2^16 bytes, latch on high clock signal
    main_ram->pins.clk.Connect(&cpu_clock_delay->pins.out); // delay this clock signal for setup and hold times to be correct
    main_ram->pins.cs_n.Connect(&address_decoder->pins.ram_cs_n); // connect the CS line to the address decoder logic
    main_ram->pins.rw_n.Connect(&cpu->pins.rw_n); // connect the read/write line from the cpu
    main_ram->pins.a.Connect(&cpu->pins.a); // put main ram on the address bus
    main_ram->pins.d.Connect(&cpu->pins.db); // put main ram on the data bus

    // TEMP let's monitor some wires
    *cpu->pins.e.signal_changed    += [](Wire*, std::optional<bool> const& new_state) { cout << "E   = " << *new_state << endl; };
    *cpu->pins.vda.signal_changed  += [](Wire*, std::optional<bool> const& new_state) { cout << "VDA = " << *new_state << endl; };
    *cpu->pins.vpa.signal_changed  += [](Wire*, std::optional<bool> const& new_state) { cout << "VPA = " << *new_state << endl; };
    *cpu->pins.mx.signal_changed   += [](Wire*, std::optional<bool> const& new_state) { cout << "MX  = " << *new_state << endl; };
    *cpu->pins.rw_n.signal_changed += [](Wire*, std::optional<bool> const& new_state) { cout << "RWn = " << *new_state << endl; };
    *cpu->pins.vp_n.signal_changed += [](Wire*, std::optional<bool> const& new_state) { cout << "VPn = " << *new_state << endl; };
    *main_ram->pins.cs_n.signal_changed += [](Wire*, std::optional<bool> const& new_state) { cout << "ram CSn = " << *new_state << endl; };
    *main_ram->pins.rw_n.signal_changed += [](Wire*, std::optional<bool> const& new_state) { cout << "ram RWn = " << *new_state << endl; };
    *main_ram->pins.d.signal_changed += [](Bus<u8>*, std::optional<u8> const& new_state) { cout << "ram D = " << *new_state << endl; };

    //system_ram = make_unique<SNESRam>();
    //cpu.pins.address_bus.connect(system_ram.pins.address_bus)
}

void SNESSystem::WaitForLastThreadCommand()
{
    while(system_thread_command != CMD_NONE) ;
}

void SNESSystem::CreateSystemThread()
{
    system_thread_command = CMD_NONE;
    system_thread = make_unique<std::thread>(std::bind(&SNESSystem::SystemThreadMain, this));
}

void SNESSystem::IssueReset()
{
    WaitForLastThreadCommand();
    {
        std::lock_guard<std::mutex> lock(system_thread_command_mutex); // wait for previous command to complete
        system_thread_command = CMD_RESET;
    }
    system_thread_command_condition.notify_one();
}

void SNESSystem::IssueExitThread()
{
    WaitForLastThreadCommand();
    {
        std::lock_guard<std::mutex> lock(system_thread_command_mutex); // wait for previous command to complete
        system_thread_command = CMD_EXIT_THREAD;
    }
    system_thread_command_condition.notify_one();
}

void SNESSystem::IssueStep()
{
    WaitForLastThreadCommand();
    {
        std::lock_guard<std::mutex> lock(system_thread_command_mutex); // wait for previous command to complete
        system_thread_command = CMD_STEP;
    }
    system_thread_command_condition.notify_one();
}

void SNESSystem::SystemThreadMain()
{
    bool running = true;

    cout << "[SNESSystem] system thread started" << endl;

    for(;running;) {
        // I'm not knowledgable enough with the STL to avoid the mutex with
        // the wait event/condition variable, but the mutex isn't needed
        // and ideally the lock wouldn't be needed either. 
        // I want system_thread_command_condition.wait()
        std::unique_lock<std::mutex> lock(system_thread_command_mutex);

        // wait until we get a command mutex
        system_thread_command_condition.wait(lock, [=, this]{ return this->system_thread_command != CMD_NONE; });
        cout << "[SNESSystem] got thread command " << system_thread_command << endl;

        switch(system_thread_command) {
        case CMD_NONE:
            break;

        case CMD_EXIT_THREAD:
            cout << "[SNESSystem] got exit thread" << endl;
            running = false;
            break;

        case CMD_RESET:
            // assert reset low, step the clock, then assert reset high
            cout << "[SNESSystem] ==RESET START==" << endl;
            reset_wire.AssertLow();
            for(int i = 0; i < 1 * SNES_CPU_CLOCK_DIVIDER; i++) {
                system_clock->Step();
            }
            reset_wire.AssertHigh();
            cout << "[SNESSystem] ==RESET DONE==" << endl;
            break;

        case CMD_STEP:
            // TODO step system clock or cpu with different commands?
            cout << "[SNESSystem] ==STEP START==" << endl;
            for(int i = 0; i < 1 * SNES_CPU_CLOCK_DIVIDER; i++) {
                system_clock->Step();
            }
            cout << "[SNESSystem] ==STEP END==" << endl;
            break;
        }

        cout << "[SNESSystem] system thread command done" << endl;
        system_thread_command = CMD_NONE;
        lock.unlock();
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

