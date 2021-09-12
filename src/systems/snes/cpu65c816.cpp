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
        else if(new_state == 1) current_state = STATE_RUNNING;
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
    // the real CPU requires clock cycle to cause reset to happen, but 
    // we're gonna emulate that logic away and require 1 clock cycle
    current_state = STATE_RESET;
}

void CPU65C816::Reset()
{
    cout << "CPU65C816 reset" << endl;

    registers.pc     = 0xFFFC;
    registers.d      = 0;
    registers.dbr    = 0;
    registers.pbr    = 0;
    registers.sh     = 0x01; // high byte of S
    registers.xh     = 0;    // high byte of X
    registers.yh     = 0;    // high byte of Y
    registers.flags &= ~CPU_FLAG_D;
    registers.flags |= (CPU_FLAG_M | CPU_FLAG_X | CPU_FLAG_I);
    registers.e      = 1;    // start in emulation mode

    // reset pins
    pins.e.AssertHigh();
    pins.mx.AssertHigh();
    pins.rw_n.AssertHigh();
    pins.vda.AssertLow();
    pins.vpa.AssertLow();
    pins.vp_n.AssertHigh();
    //pins.db.HighZ();

    // next instruction is a vector fetch
    vector_pull = true;
}

void CPU65C816::ClockLow()
{
    static auto step_reset = [=, this]() {
        Reset();

        // set the next instruction cycle to fetch
        instruction_cycle = IC_VECTOR_PULL;
        SetupPinsLowCycle();
    };

    static auto step_running = [=, this]() {
        cout << "running low" << endl;
    };

    static std::function<void()> step[] = {
        step_reset,
        step_running,
    };

    step[current_state]();
}

void CPU65C816::ClockHigh()
{
    static auto step_reset = [=, this]() {
        // set up the opcode fetch cycle
        SetupPinsHighCycle();
    };

    static auto step_running = [=, this]() {
        cout << "running high" << endl;
    };

    static std::function<void()> step[] = {
        step_reset,
        step_running,
    };

    step[current_state]();
}

void CPU65C816::SetupPinsLowCycle()
{
    switch(instruction_cycle) {
    case IC_OPCODE_FETCH:
        break;
    case IC_VECTOR_PULL:
        //pins.db.Assert(registers.pbr);   // put program bank on the data lines
        //pins.a.Assert(registers.pc);     // put the pc register on the address lines 
        pins.rw_n.AssertHigh();          // assert read
        pins.vda.AssertHigh();           // vda and vpa high means op-code fetch
        pins.vpa.AssertHigh();           // ..
        if(vector_pull) {                // for vector pull assert VP low
            pins.vp_n.AssertLow();
            vector_pull = false;
        }
        break;
    default:
        break;
    }
}

void CPU65C816::SetupPinsHighCycle()
{
    switch(instruction_cycle) {
    case IC_OPCODE_FETCH:
    case IC_VECTOR_PULL:
        //pins.db.HighZ();  // put data bus into high Z for reading
        break;
    default:
        break;
    }
}

