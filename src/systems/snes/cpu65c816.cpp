#include <functional>
#include <iostream>
#include <iomanip>

#include "systems/snes/cpu65c816.h"

using namespace std;

// The CPU defaults to running state until you pull reset low
CPU65C816::CPU65C816()
{
    // reset the CPU on the falling edge
    *pins.reset_n.signal_changed += [=, this](Wire* driver, std::optional<bool> const& new_state) {
        // the real CPU requires clock cycle to cause reset to happen, but 
        // we're gonna emulate that logic away and listen to the falling edge
        if(!*new_state) this->Reset();
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
        if(*new_state) this->SetupPinsHighCycle();
        else           this->SetupPinsLowCycle();
    };
}

CPU65C816::~CPU65C816()
{
}

void CPU65C816::Reset()
{
    cout << "[cpu65c816] cpu reset" << endl;

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

    // set the next instruction cycle to fetch the reset vector
    next_instruction_cycle = IC_VECTOR_PULL_LOW;
    data_fetch_address = 0xFFFC;
}

void CPU65C816::ClockFallingEdge()
{
    // sample the data line, always
    u8 data_line = pins.db.Sample();

    // move the instruction cycle to the next one
    instruction_cycle = next_instruction_cycle;

    cout << "[cpu65c816] CPU step LOW -- data line = $" << setfill('0') << setw(2) << right << hex << (u16)data_line << endl;

    switch(instruction_cycle) {
    case IC_VECTOR_PULL_LOW:
        // nothing to do in this cycle but set up signals. next cycle will be the high byte
        next_instruction_cycle = IC_VECTOR_PULL_HIGH;
        break;

    case IC_VECTOR_PULL_HIGH:
        // latch the low vector address
        registers.pc = (registers.pc & 0xFF00) | data_line;

        // move to fetch the high byte
        next_instruction_cycle = IC_VECTOR_FETCH_OPCODE;
        break;

    case IC_VECTOR_FETCH_OPCODE:
        // latch the high vector address
        registers.pc = (registers.pc & 0x00FF) | (data_line << 8);

        // TEMP -- should set the next opcode, but this one switches to dead
        next_instruction_cycle = IC_DECODE;
        break;

    case IC_DECODE:
        // latch the IR
        registers.ir = data_line;

        // TEMP -- should pick the microcode to execute
        instruction_cycle = IC_DEAD;
        break;

    case IC_DEAD:
        break;
    }

    // finally, de-assert necessary pins so all devices release the data bus
    // this will cause the address decoder to make all CSn lines high
    pins.vda.AssertLow();
    pins.vpa.AssertLow();
    pins.vp_n.AssertHigh();
    pins.rw_n.AssertHigh();
}

void CPU65C816::ClockRisingEdge()
{
    cout << "[cpu65c816] CPU step HIGH -- data line = " << setfill('0') << setw(2) << right << hex << pins.db.Sample() << endl;
}

void CPU65C816::SetupPinsLowCycle()
{
    switch(instruction_cycle) {
    case IC_VECTOR_FETCH_OPCODE:
    case IC_OPCODE_FETCH:
        cout << "asserting opcode fetch lines" << endl;
        pins.vda.AssertHigh();           // vda and vpa high means op-code fetch
        pins.vpa.AssertHigh();           // ..
        pins.rw_n.AssertHigh();          // assert read

        // do data and address after VDA/VPA/RWn
        pins.db.Assert(registers.pbr);   // opcode fetch uses program bank

        // put the PC address on the address lines 
        pins.a.Assert(registers.pc);
        break;

    case IC_VECTOR_PULL_LOW:
    case IC_VECTOR_PULL_HIGH:
        cout << "asserting vector pull lines" << endl;
        pins.vda.AssertHigh();           // vda and vpa high means op-code fetch
        pins.vpa.AssertHigh();           // ..
        pins.rw_n.AssertHigh();          // assert read
        pins.vp_n.AssertLow();           // for vector pull assert VP low

        // do data and address after VDA/VPA/RWn
        pins.db.Assert(0x00);            // vector fetch is always bank 0

        // put the vector address on the address lines 
        pins.a.Assert(data_fetch_address + ((instruction_cycle == IC_VECTOR_PULL_HIGH) ? 1 : 0));
        break;

    case IC_DEAD:
        break;

    default:
        break;
    }
}

void CPU65C816::SetupPinsHighCycle()
{
    // on a read cycle, we need to de-assert the db bus
    // on a write cycle, we need to change the data on the db bus
    // TODO IsReadCycle() ?
    switch(instruction_cycle) {
    case IC_OPCODE_FETCH:
    case IC_VECTOR_FETCH_OPCODE:
    case IC_VECTOR_PULL_LOW:
    case IC_VECTOR_PULL_HIGH:
        pins.db.HighZ();
        break;

    case IC_DEAD:
        break;
    }
}

