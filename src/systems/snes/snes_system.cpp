#include <cassert>
#include <chrono>
#include <iomanip>
#include <fstream>

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

    // the cpu will latch data from the data bus on the non-delayed phi2 clock
    // and so we need to wait to set up the new address signals
    cpu_signal_setup_delay = make_unique<SignalDelay<bool>>(false, 1, SNES_CPU_CLOCK_DIVIDER/2); // on the falling edge, delay the signal by 1, and reset the counter every half cpu clock cycle
    cpu_signal_setup_delay->pins.clk.Connect(&system_clock->pins.out);
    cpu_signal_setup_delay->pins.in.Connect(&cpu_clock->pins.out);
    cpu_signal_setup_delay->Transfer(); // transfer the dead state of the clock right now

    // and we'll also need a delay for RAM and peripherals to set up the data bus before
    // the CPU latches, so that's one system clock before phi2 falls
    peripheral_clock = make_unique<SignalDelay<bool>>(false, 2, SNES_CPU_CLOCK_DIVIDER/2); // on the falling edge, delay the signal by 2, and reset the counter every half cpu clock cycle
    peripheral_clock->pins.clk.Connect(&system_clock->pins.out);
    peripheral_clock->pins.in.Connect(&cpu_clock->pins.out);
    peripheral_clock->Transfer(); // transfer the dead state of the clock right now

    // create the cpu
    cpu = make_unique<CPU65C816>();

    // connect CPU to the system
    cpu->pins.phi2.Connect(&cpu_clock->pins.out); // connect the clock
    cpu->pins.signal_setup.Connect(&cpu_signal_setup_delay->pins.out); // connect the signal setup lines

    // create the address decoder, which also contains the data bank latch and data transceiver
    address_decoder = make_unique<SNESAddressDecoder>();
    address_decoder->pins.phi2.Connect(&cpu_clock->pins.out);
    address_decoder->pins.rw_n.Connect(&cpu->pins.rw_n);
    address_decoder->pins.vda.Connect(&cpu->pins.vda);
    address_decoder->pins.vpa.Connect(&cpu->pins.vpa);
    address_decoder->pins.db.Connect(&cpu->pins.db);
    address_decoder->pins.a_in.Connect(&cpu->pins.a);

    // create the main ram
    main_ram = make_unique<RAM<u32, u8>>(13, true);               // 2^13 bytes (8K), 32 (technically 24-) bit address space, latch on high clock signal
    main_ram->pins.clk.Connect(&peripheral_clock->pins.out);      // get the peripherals clocking at the end of phi2 high
    main_ram->pins.cs_n.Connect(&address_decoder->pins.ram_cs_n); // connect the CS line to the address decoder logic
    main_ram->pins.a.Connect(&address_decoder->pins.a_out);       // connect the address lines to the address decoder
    main_ram->pins.d.Connect(&address_decoder->pins.d);           // connect the data bus to the data transceiver
    main_ram->pins.rw_n.Connect(&cpu->pins.rw_n);                 // connect the read/write line from the cpu

    // connect the main rom, which was created in CreateNewProjectFromFile
    main_rom = make_unique<ROM<u32, u8>>(15, true);               // 2^15 bits, 24-bit address space, latch on high clock signal
    main_rom->pins.clk.Connect(&peripheral_clock->pins.out);      // get the peripherals clocking at the end of phi2 high
    main_rom->pins.cs_n.Connect(&address_decoder->pins.rom_cs_n); // connect the CS line to the address decoder logic
    main_rom->pins.a.Connect(&address_decoder->pins.a_out);       // connect the address lines to the address decoder
    main_rom->pins.d.Connect(&address_decoder->pins.d);           // connect the data bus to the data transceiver

    // connect the reset line to the system
    reset_wire.AssertHigh();  // default reset to high when everything is attached
    cpu->pins.reset_n.Connect(&reset_wire);                       // CPU reset is synchronous and needs clock cycles
    address_decoder->pins.reset_n.Connect(&reset_wire);           // address decoder reset immediately so that all the CSn lines get 
                                                                  // deasserted before all the clocks reset
    cpu_clock->pins.reset_n.Connect(&reset_wire);                 // now reset all the clocks and delay signals
    peripheral_clock->pins.reset_n.Connect(&reset_wire);
    cpu_signal_setup_delay->pins.reset_n.Connect(&reset_wire);

    // TEMP let's monitor some wires
    //!*cpu->pins.e.signal_changed    += [](Wire*, std::optional<bool> const& new_state) { cout << "E   = " << *new_state << endl; };
    //!*cpu->pins.vda.signal_changed  += [](Wire*, std::optional<bool> const& new_state) { cout << "VDA = " << *new_state << endl; };
    //!*cpu->pins.vpa.signal_changed  += [](Wire*, std::optional<bool> const& new_state) { cout << "VPA = " << *new_state << endl; };
    //!*cpu->pins.mx.signal_changed   += [](Wire*, std::optional<bool> const& new_state) { cout << "MX  = " << *new_state << endl; };
    //!*cpu->pins.rw_n.signal_changed += [](Wire*, std::optional<bool> const& new_state) { cout << "RWn = " << *new_state << endl; };
    //!*cpu->pins.vp_n.signal_changed += [](Wire*, std::optional<bool> const& new_state) { cout << "VPn = " << *new_state << endl; };
    //!*main_ram->pins.cs_n.signal_changed += [](Wire*, std::optional<bool> const& new_state) { cout << "ram CSn = " << *new_state << endl; };
    //!*main_ram->pins.rw_n.signal_changed += [](Wire*, std::optional<bool> const& new_state) { cout << "ram RWn = " << *new_state << endl; };
    //!*main_ram->pins.d.signal_changed += [](Bus<u8>*, std::optional<u8> const& new_state) { cout << "ram D = $" << hex << setw(2) << (u16)*new_state << endl; };
}

void SNESSystem::WaitForLastThreadCommand()
{
    system_thread_stop_clock = true; // if in the RUN state, stop
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

void SNESSystem::IssueStepSystem()
{
    WaitForLastThreadCommand();
    {
        std::lock_guard<std::mutex> lock(system_thread_command_mutex); // wait for previous command to complete
        system_thread_command = CMD_STEP_SYSTEM;
    }
    system_thread_command_condition.notify_one();
}

void SNESSystem::IssueStepCPU()
{
    WaitForLastThreadCommand();
    {
        std::lock_guard<std::mutex> lock(system_thread_command_mutex); // wait for previous command to complete
        system_thread_command = CMD_STEP_CPU;
    }
    system_thread_command_condition.notify_one();
}

void SNESSystem::IssueRun()
{
    WaitForLastThreadCommand();
    {
        std::lock_guard<std::mutex> lock(system_thread_command_mutex); // wait for previous command to complete
        system_thread_command = CMD_RUN;
        system_thread_stop_clock = false; // clear any previous stop
    }
    system_thread_command_condition.notify_one();
}

void SNESSystem::IssueStop()
{
    WaitForLastThreadCommand();
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
            reset_wire.AssertHigh();
            for(int i = 0; i < 1 * SNES_CPU_CLOCK_DIVIDER; i++) {
                system_clock->Step();
            }
            cout << "[SNESSystem] ==RESET DONE==" << endl;
            break;

        case CMD_STEP_SYSTEM:
            cout << "[SNESSystem] ==STEP SYSTEM START==" << endl;
            system_clock->Step();
            cout << "[SNESSystem] ==STEP SYSTEM END==" << endl;
            break;

        case CMD_STEP_CPU:
            cout << "[SNESSystem] ==STEP CPU START==" << endl;
            for(int i = 0; i < 1 * SNES_CPU_CLOCK_DIVIDER; i++) {
                system_clock->Step();
            }
            cout << "[SNESSystem] ==STEP CPU END==" << endl;
            break;

        case CMD_RUN:
            cout << "[SNESSystem] ==RUN START==" << endl;
            {
                // run the CPU and count the cycles
                auto startTime = std::chrono::steady_clock::now();
                u64 clockSteps = 0;
                for(;!system_thread_stop_clock;) {
                    system_clock->Step();
                    clockSteps += 1;
                }

                // print out the execution speed
                auto stopTime = std::chrono::steady_clock::now();
                double deltaTime = (stopTime - startTime) / 1.0s;
                cout << "[SNESSystem] ran " << clockSteps << " master clock cycles in " << deltaTime << " s ";
                double clocksPerSecond = (double)clockSteps / deltaTime;
                cout << "(" << clocksPerSecond << " cycles/sec)" << endl;
            }
            cout << "[SNESSystem] ==RUN END==" << endl;
            break;
        }

        system_thread_command = CMD_NONE;
        lock.unlock();
    }
}

bool SNESSystem::CreateNewProjectFromFile(string const& file_path_name)
{
    // TODO move the loader into its own class once it becomes complex enough
    // for now this basic image loader is fine
    string lcase_file_path_name = StringLower(file_path_name);
    assert(StringEndsWith(lcase_file_path_name, ".bin")); // other formats not yet supported

    rom_file_path_name = file_path_name;
    ifstream is(rom_file_path_name, ios::binary);

    u16 rom_size;
    u16 load_address;
    is.read((char *)&load_address, sizeof(load_address));
    is.read((char *)&rom_size, sizeof(rom_size));
    cout << "loading rom size $" << hex << rom_size << " bytes to address $" << load_address << endl;

    u8* rom_image = new u8[rom_size];
    is.read((char *)rom_image, rom_size);

    // load the image
    main_rom->LoadImage(rom_image, load_address, rom_size);

    delete [] rom_image;
    return true;
}

bool SNESSystem::IsROMValid(std::string const& file_path_name, std::istream& is)
{
    string lcase_file_path_name = StringLower(file_path_name);

    if(StringEndsWith(lcase_file_path_name, ".bin")) {
        return true;
    } else if(StringEndsWith(lcase_file_path_name, ".smc")) {
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

