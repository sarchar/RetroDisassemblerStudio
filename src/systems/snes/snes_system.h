#pragma once

#include <condition_variable>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "system_clock.h"
#include "systems/system.h"
#include "systems/snes/cpu65c816.h"

#define SNES_CLOCK_FREQUENCY 22477000

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

    // System components
    Wire reset_wire { "reset" };
    std::unique_ptr<SystemClock> system_clock;
    std::unique_ptr<CPU65C816> cpu;
    //std::unique_ptr<SNESRam> system_ram;

    std::unique_ptr<std::thread> system_thread;
    std::condition_variable cond_start_clock;
    int system_thread_command;
};
