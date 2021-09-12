#include <functional>
#include <iostream>

#include "systems/snes/cpu65c816.h"

using namespace std;

// The CPU defaults to running state until you pull reset low
CPU65C816::CPU65C816()
    : current_state(STATE_RUNNING)
{
    // reset the CPU on the falling edge
    *pins.reset_n.signal_changed += [=, this](Wire* driver, tristate new_state) {
        if(new_state == 0) this->StartReset();
        else if(new_state == 1 && this->current_state == STATE_PRE_RESET) current_state = STATE_RESET;
    };

    // capture both rising and falling edges of the PHI2 signal
    static std::function<void()> clock_handlers[] = {
        [](){},
        std::bind(&CPU65C816::ClockLow, this),
        std::bind(&CPU65C816::ClockHigh, this),
    };

    *pins.phi2.signal_changed += [=, this](Wire* driver, tristate new_state) {
        clock_handlers[new_state + 1]();
    };
}

CPU65C816::~CPU65C816()
{
}

void CPU65C816::StartReset()
{
    // reset signal needs to be held low for two full clock cycles
    current_state = STATE_PRE_RESET;
}

void CPU65C816::ClockLow()
{
    cout << "CPU65C816 clock low" << endl;
}

void CPU65C816::ClockHigh()
{
    cout << "CPU65C816 clock high" << endl;
}

