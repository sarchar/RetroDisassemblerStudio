#include <functional>
#include <iostream>
#include <iomanip>

#include "systems/snes/cpu65c816.h"

using namespace std;

// The CPU defaults to running state until you pull reset low
CPU65C816::CPU65C816()
    : current_state(STATE_RUNNING)
{
    // reset the CPU on the falling edge
    *pins.reset_n.signal_changed += [=, this](Wire* driver, std::optional<bool> const& new_state) {
        if(!*new_state) this->StartReset();
        else            current_state = STATE_RUNNING;
    };

    // capture both rising and falling edges of the PHI2 signal
    *pins.phi2.signal_changed += [=, this](Wire* driver, std::optional<bool> const& new_state) {
        assert(new_state.has_value());
        if(*new_state) this->ClockRisingEdge();
        else           this->ClockFallingEdge();
    };

    // capture both rising and falling edges of the PHI2 setup signal
    *pins.signal_setup.signal_changed += [=, this](Wire* driver, std::optional<bool> const& new_state) {
        assert(new_state.has_value());
        if(*new_state) this->SetupPinsLowCycle();
        else           this->SetupPinsHighCycle();
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
    cout << "[cpu65c816] cpu reset" << endl;

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
    pins.db.HighZ();
}

void CPU65C816::ClockFallingEdge()
{
    // TODO latch data line
    switch(current_state) {
    case STATE_RESET:
        Reset();

        // set the next instruction cycle to fetch
        instruction_cycle = IC_VECTOR_PULL_LOW;
        break;

    case STATE_RUNNING:
        cout << "[cpu65c816] CPU step LOW -- data line = $" << setfill('0') << setw(2) << right << hex << pins.db.Sample() << endl;

        switch(instruction_cycle) {
        case IC_VECTOR_PULL_LOW:
            // next instruction cycle after reset will be high byte vector
            registers.pc += 1;
            instruction_cycle = IC_VECTOR_PULL_HIGH;
            break;

        case IC_VECTOR_PULL_HIGH:
            instruction_cycle = IC_DEAD;
            break;

        case IC_DEAD:
            break;
        }

        break;
    }

    // finally, de-assert necessary pins so all devices release the data bus
    pins.vda.AssertLow();
    pins.vpa.AssertLow();
    pins.vp_n.AssertHigh();
    pins.rw_n.AssertHigh();
}

void CPU65C816::ClockRisingEdge()
{
    switch(current_state) {
    case STATE_RESET:
        break;

    case STATE_RUNNING:
        cout << "[cpu65c816] CPU step HIGH -- data line = " << setfill('0') << setw(2) << right << hex << pins.db.Sample() << endl;

        // on a read cycle, we need to de-assert the db bus
        // on a write cycel, we need to change the data on the db bus
        // TODO IsReadCycle() ?
        switch(instruction_cycle) {
        case IC_OPCODE_FETCH:
        case IC_VECTOR_PULL_LOW:
        case IC_VECTOR_PULL_HIGH:
            pins.db.HighZ();
            break;

        case IC_DEAD:
            break;
        }
        break;
    }

}

void CPU65C816::SetupPinsLowCycle()
{
    switch(instruction_cycle) {
    case IC_OPCODE_FETCH:
        break;

    case IC_VECTOR_PULL_LOW:
    case IC_VECTOR_PULL_HIGH:
        pins.vda.AssertHigh();           // vda and vpa high means op-code fetch
        pins.vpa.AssertHigh();           // ..
        pins.rw_n.AssertHigh();          // assert read
        pins.vp_n.AssertLow();           // for vector pull assert VP low

        // do data and address after VDA/VPA/RWn
        pins.db.Assert(registers.pbr);   // put program bank on the data lines
        pins.a.Assert(registers.pc);     // put the pc register on the address lines 
        break;

    default:
        break;
    }
}

void CPU65C816::SetupPinsHighCycle()
{
    switch(instruction_cycle) {
    case IC_OPCODE_FETCH:
    case IC_VECTOR_PULL_LOW:
    case IC_VECTOR_PULL_HIGH:
        pins.db.HighZ();  // put data bus into high Z for reading
        break;

    default:
        break;
    }
}

