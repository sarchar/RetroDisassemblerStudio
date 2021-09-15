#include <functional>
#include <iostream>
#include <iomanip>

#include "systems/snes/cpu65c816.h"

using namespace std;

CPU65C816::INSTRUCTION_CYCLE const
    CPU65C816::VECTOR_PULL_UC[] = { IC_VECTOR_PULL_LOW, IC_VECTOR_PULL_HIGH, IC_OPCODE_FETCH };

CPU65C816::INSTRUCTION_CYCLE const
    CPU65C816::JMP_UC[] = { IC_WORD_IMM_LOW, IC_WORD_IMM_HIGH, IC_STORE_PC_OPCODE_FETCH };

CPU65C816::INSTRUCTION_CYCLE const CPU65C816::DEAD_INSTRUCTION[] = { IC_DEAD };

CPU65C816::INSTRUCTION_CYCLE const * const CPU65C816::INSTRUCTION_UCs[256] = {
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    JMP_UC          , DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
};

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

    // set the next instruction cycle to fetch the reset vector and execute
    current_instruction_cycle_set = &CPU65C816::VECTOR_PULL_UC[0];
    current_instruction_cycle_set_pc = 0;
    data_fetch_address = 0xFFFC;
}


void CPU65C816::ClockFallingEdge()
{
    // sample the data line, always
    u8 data_line = pins.db.Sample();
    cout << "[cpu65c816] CPU step LOW -- data line = $" << setfill('0') << setw(2) << right << hex << (u16)data_line << endl;

    // finish the previous cycle
    FinishInstructionCycle(data_line);

    // move the instruction cycle to the next one
    instruction_cycle = current_instruction_cycle_set[current_instruction_cycle_set_pc++];

    // start the next cycle
    StartInstructionCycle();

    // finally, de-assert necessary pins so all devices release the data bus
    // this will cause the address decoder to make all CSn lines high
    pins.vda.AssertLow();
    pins.vpa.AssertLow();
    pins.rw_n.AssertHigh();
}

// called at the beginning of the new clock cycle (phi2 falling edge)
// and generally just latches data
void CPU65C816::FinishInstructionCycle(u8 data_line)
{
    switch(instruction_cycle) {
    case IC_VECTOR_PULL_LOW:
        // latch the low vector address
        registers.pc = (registers.pc & 0xFF00) | data_line;
        break;

    case IC_VECTOR_PULL_HIGH:
        // latch the high vector address
        registers.pc = (registers.pc & 0x00FF) | (data_line << 8);

        // de-assert VPn after we fetch the high byte of the vector
        pins.vp_n.AssertHigh();
        break;

    case IC_OPCODE_FETCH:
    case IC_STORE_PC_OPCODE_FETCH:
        // latch the instruction register
        registers.ir = data_line;

        // increment program counter
        registers.pc += 1;

        // pick the microcode to run
        current_instruction_cycle_set = &CPU65C816::INSTRUCTION_UCs[registers.ir][0];
        current_instruction_cycle_set_pc = 0;
        break;

    case IC_WORD_IMM_LOW:
        // latch the word immediate low byte
        cout << "data_line = " << hex << setw(2) << data_line << endl;
        word_immediate = (word_immediate & 0xFF00) | data_line;
        cout << "word_immediate = " << hex << setw(4) << word_immediate << endl;

        // increment PC
        registers.pc += 1;
        break;

    case IC_WORD_IMM_HIGH:
        // latch the word immediate high byte
        word_immediate = (word_immediate & 0x00FF) | (data_line << 8);
        cout << "word_immediate = " << hex << setw(4) << word_immediate << endl;

        // increment PC
        registers.pc += 1;
        break;

    case IC_DEAD:
        break;
    }
}

void CPU65C816::StartInstructionCycle()
{
    switch(instruction_cycle) {
    case IC_VECTOR_PULL_LOW:
    case IC_VECTOR_PULL_HIGH:
    case IC_WORD_IMM_LOW:
    case IC_WORD_IMM_HIGH:
        // nothing to do to start this cycle but set up signals, which happens in SetupPinsLowCycle()
        break;

    case IC_OPCODE_FETCH:
        // nothing to do to start this cycle but set up signals, which happens in SetupPinsLowCycle()
        break;

    case IC_STORE_PC_OPCODE_FETCH:
        // at the beginning of the store-pc-opcode-fetch cycle, 
        // store the memory value in PC. PC is 16-bit, so take word_immediate
        // and then fetch the opcode at that address
        registers.pc = word_immediate;
        break;

    case IC_DEAD:
        break;
    }
}

void CPU65C816::ClockRisingEdge()
{
    cout << "[cpu65c816] CPU step HIGH -- data line = " << setfill('0') << setw(2) << right << hex << pins.db.Sample() << endl;
}

void CPU65C816::SetupPinsLowCycle()
{
    switch(instruction_cycle) {
    case IC_OPCODE_FETCH:
    case IC_STORE_PC_OPCODE_FETCH:
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

    case IC_WORD_IMM_LOW:
    case IC_WORD_IMM_HIGH:
        cout << "asserting word immediate pull lines" << endl;
        pins.vda.AssertLow();            // assert only program data high
        pins.vpa.AssertHigh();           // ..
        pins.rw_n.AssertHigh();          // assert read

        // do data and address after VDA/VPA/RWn
        pins.db.Assert(registers.pbr);   // operands are in program storage

        // put the PC address on the address lines 
        pins.a.Assert(registers.pc);
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
    if(IsReadCycle()) {
        pins.db.HighZ();
    } else if(IsWriteCycle()) { // on a write cycle, we need to change the data on the db bus
    }
}

