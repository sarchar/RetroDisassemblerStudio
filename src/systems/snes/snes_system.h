#pragma once

#include <condition_variable>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "clock_divider.h"
#include "ram.h"
#include "signal_delay.h"
#include "system_clock.h"
#include "systems/system.h"
#include "systems/snes/cpu65c816.h"
#include "systems/snes/snes_address_decoder.h"

#define SNES_CLOCK_FREQUENCY 22477000
#define SNES_CPU_CLOCK_DIVIDER 6

class SNESSystem : public System {
public:
    SNESSystem();
    virtual ~SNESSystem();

    System::Information const* GetInformation();
    bool LoadROM(std::string const&);

    // creation interface
    static System::Information const* GetInformationStatic();
    static bool IsROMValid(std::string const& file_path_name, std::istream& is);
    static std::shared_ptr<System> CreateSystem();

private:
    void BuildSystemComponents();
    void CreateSystemThread();
    void SystemThreadMain();

    void WaitForLastThreadCommand();
    void IssueReset();
    void IssueExitThread();
    void IssueStep();

    // System components
    Wire reset_wire { "SNESSytem.reset" };
    std::unique_ptr<SystemClock> system_clock;
    std::unique_ptr<ClockDivider> cpu_clock;
    std::unique_ptr<SignalDelay<bool>> cpu_clock_delay;
    std::unique_ptr<SignalDelay<bool>> cpu_signal_setup_delay;
    std::unique_ptr<CPU65C816> cpu;
    //std::unique_ptr<SNESRam> system_ram;
    std::unique_ptr<SNESAddressDecoder> address_decoder;
    std::unique_ptr<RAM<u16,u8>> main_ram;

    // Threaded system 
    std::unique_ptr<std::thread> system_thread;
    std::mutex system_thread_command_mutex;
    std::condition_variable system_thread_command_condition;

    enum {
        CMD_NONE,
        CMD_RESET,
        CMD_EXIT_THREAD,
        CMD_STEP
    } system_thread_command;
};
