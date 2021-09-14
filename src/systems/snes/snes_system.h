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

    std::string const& GetROMFilePathName() const { return rom_file_path_name; }

    // creation interface
    static System::Information const* GetInformationStatic();
    static bool IsROMValid(std::string const& file_path_name, std::istream& is);
    static std::shared_ptr<System> CreateSystem();

    // Debugging interface
public:
    void IssueReset();
    void IssueExitThread();
    void IssueStepSystem();
    void IssueStepCPU();

    inline u8  GetE()     const { return cpu->GetE();     }
    inline u8  GetFlags() const { return cpu->GetFlags(); }
    inline u16 GetPC()    const { return cpu->GetPC();    }
    inline u8  GetA()     const { return cpu->GetA();     }
    inline u16 GetC()     const { return cpu->GetC();     }
    inline u16 GetX()     const { return cpu->GetX();     }
    inline u8  GetXL()    const { return cpu->GetXL();    }
    inline u16 GetY()     const { return cpu->GetY();     }
    inline u8  GetYL()    const { return cpu->GetYL();    }

    inline std::optional<bool> const& GetSignalRWn() const { return cpu->pins.rw_n.Get(); }
    inline std::optional<bool> const& GetSignalVPn() const { return cpu->pins.vp_n.Get(); }

    inline std::optional<bool> const& GetSignalVDA() const { return cpu->pins.vda.Get(); }
    inline std::optional<bool> const& GetSignalVPA() const { return cpu->pins.vpa.Get(); }

    inline std::optional<bool> const& GetSignalE()   const { return cpu->pins.e.Get(); }
    inline std::optional<bool> const& GetSignalMX()  const { return cpu->pins.mx.Get(); }

    inline std::optional<u8>   const& GetSignalDB()  const { return cpu->pins.db.Get(); }
    inline std::optional<u16>  const& GetSignalA()   const { return cpu->pins.a.Get(); }

    inline std::optional<bool> const& GetSignalRAMCSn() const { return main_ram->pins.cs_n.Get(); }
private:
    void BuildSystemComponents();
    void CreateSystemThread();
    void SystemThreadMain();

    void WaitForLastThreadCommand();

    std::string rom_file_path_name;

    // System components
    Wire reset_wire { "SNESSystem.reset" };
    std::unique_ptr<SystemClock> system_clock;
    std::unique_ptr<ClockDivider> cpu_clock;
    std::unique_ptr<SignalDelay<bool>> peripheral_clock;
    std::unique_ptr<SignalDelay<bool>> cpu_signal_setup_delay;
    std::unique_ptr<CPU65C816> cpu;
    std::unique_ptr<SNESAddressDecoder> address_decoder;
    std::unique_ptr<RAM<u32,u8>> main_ram;

    // Threaded system 
    std::unique_ptr<std::thread> system_thread;
    std::mutex system_thread_command_mutex;
    std::condition_variable system_thread_command_condition;

    enum {
        CMD_NONE,
        CMD_RESET,
        CMD_EXIT_THREAD,
        CMD_STEP_SYSTEM,
        CMD_STEP_CPU
    } system_thread_command;
};
