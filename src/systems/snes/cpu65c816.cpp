#include <functional>
#include <iostream>
#include <iomanip>

#include "systems/snes/cpu65c816.h"

using namespace std;

CPU65C816::UC_OPCODE const
    CPU65C816::INC_UC[]           = { UC_FETCH_MEMORY | UC_INC | UC_STORE_MEMORY, UC_FETCH_OPCODE };

CPU65C816::UC_OPCODE const
    CPU65C816::JMP_UC[]           = { UC_FETCH_MEMORY | UC_NOP | UC_STORE_PC    , UC_FETCH_OPCODE };

CPU65C816::UC_OPCODE const
    CPU65C816::LDA_UC[]           = { UC_FETCH_MEMORY | UC_NOP | UC_STORE_A     , UC_FETCH_OPCODE };

CPU65C816::UC_OPCODE const
    CPU65C816::NOP_UC[]           = { UC_FETCH_A      | UC_NOP                  , UC_FETCH_OPCODE };

CPU65C816::UC_OPCODE const 
    CPU65C816::DEAD_INSTRUCTION[] = { UC_DEAD };

CPU65C816::UC_OPCODE const * const CPU65C816::INSTRUCTION_UCs[256] = {
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, INC_UC          , DEAD_INSTRUCTION, 
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
    DEAD_INSTRUCTION, LDA_UC          , DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
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
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, NOP_UC          , DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
};

CPU65C816::ADDRESSING_MODE const CPU65C816::INSTRUCTION_ADDRESSING_MODES[256] = {
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_ACCUMULATOR, AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMMEDIATE_WORD, AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMMEDIATE_BYTE, AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
    AM_IMPLIED       , AM_IMPLIED       , AM_IMPLIED    , AM_IMPLIED,
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
    current_uc_opcode       = UC_FETCH_A | UC_NOP; // this makes the next FinishInstructionCycle() do nothing
    current_uc_set          = &CPU65C816::JMP_UC[0];
    current_uc_set_pc       = 0;
    current_addressing_mode = AM_VECTOR;
    current_memory_step     = MS_INIT;
    vector_address          = 0xFFFC;
    vector_pull             = true;
}


void CPU65C816::ClockFallingEdge()
{
    // sample the data line, always
    u8 data_line = pins.db.Sample();
    cout << "[cpu65c816] CPU step LOW -- data line = $" << setfill('0') << setw(2) << right << hex << (u16)data_line << endl;

    // finish the previous cycle
    FinishInstructionCycle(data_line);

    // move the microcode cycle to the next one
    current_uc_opcode = current_uc_set[current_uc_set_pc++];

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
    // apply any memory fetch and store operations
    switch(current_uc_opcode & 0x03) {
    case UC_FETCH_MEMORY:
        switch(current_memory_step) {
        case MS_FETCH_VECTOR_LOW:
            // latch the vector low byte
            memory_word = (memory_word & 0xFF00) | data_line;

            // move to the next vector byte
            vector_address += 1;

            // get the high byte and keep running UC_FETCH_MEMORY
            current_memory_step = MS_FETCH_VECTOR_HIGH;
            current_uc_set_pc--;
            break;

        case MS_FETCH_VECTOR_HIGH:
            // latch the vector high byte
            memory_word = (memory_word & 0x00FF) | (data_line << 8);

            // finished a vector pull
            vector_pull = false;

            // we're done with memory, so we can move on
            current_memory_step = MS_DONE; // store the operand if needed
            break;

        case MS_FETCH_OPERAND_LOW:
            // latch the operand low byte
            memory_word = (memory_word & 0xFF00) | data_line;

            // increase PC as that's where OPERAND_LOW comes from
            registers.pc += 1;

            // move onto high byte if necessasry
            switch(current_addressing_mode) {
            case AM_IMMEDIATE_WORD:
                // read the LOW byte and want a word, so stay in memory fetch
                current_memory_step = MS_FETCH_OPERAND_HIGH;
                current_uc_set_pc--; 
                break;

            default:
                // all other cases are done
                current_memory_step = MS_DONE; // so we can store the operand if requested
                break;
            }
            break;

        case MS_FETCH_OPERAND_HIGH:
            // latch the operand high byte
            memory_word = (memory_word & 0x00FF) | (data_line << 8);

            // increase PC as that's where OPERAND_HIGH comes from
            registers.pc += 1;

            // TODO move onto bank byte if necessasry
            switch(current_addressing_mode) {
            //!case AM_IMMEDIATE_LONG:
            //!    // read the LOW byte and want a word, so stay in memory fetch
            //!    current_memory_step = MS_FETCH_OPERAND_BANK;
            //!    current_uc_set_pc--; 
            //!    break;

            default:
                // all other cases are done
                current_memory_step = MS_DONE; // so we can store the operand if requested
                break;
            }
            break;

        }

        break;

    case UC_FETCH_OPCODE:
        // store opcode in IR
        registers.ir = data_line;

        // increment the program counter
        registers.pc += 1;

        // determine addressing mode
        current_addressing_mode = CPU65C816::INSTRUCTION_ADDRESSING_MODES[registers.ir];

        // select microcode and reset counter
        current_uc_set = &CPU65C816::INSTRUCTION_UCs[registers.ir][0];
        current_uc_set_pc = 0;
        break;
    }


    if(current_memory_step == MS_DONE) {
        // finish operation first
        switch(current_uc_opcode & 0x1C0) {
        case UC_DEAD:
            // prevent the instruction from moving on by keeping the microcode fixed at 0
            current_uc_set_pc = 0;
            break;

        case UC_NOP:
            break;

        case UC_INC:
            memory_byte += 1;
            break;
        }

        // TODO current_memory_step needs a MS_WRITE ?
        switch(current_uc_opcode & 0x38) {
        case UC_STORE_MEMORY:
            switch(current_addressing_mode) {
            case AM_ACCUMULATOR:
                cout << "[cpu65c816] storing byte into A (AM_ACCUMULATOR)" << endl;
                registers.a = memory_byte;
                break;
            default:
                assert(false); //unimplmeneted addressing mode for UC_STORE_MEMORY
                break;
            }
            break;

        case UC_STORE_PC:
            cout << "[cpu65c816] storing word immediate into PC" << endl;
            registers.pc = memory_word;
            break;

        case UC_STORE_A:
            cout << "[cpu65c816] storing byte immediate into A" << endl;
            registers.a = memory_word & 0xFF;
            break;
        }

        // and reset
        current_memory_step = MS_INIT;
    }
}

void CPU65C816::StartInstructionCycle()
{
    // apply memory model
    switch(current_uc_opcode & 0x03) {
    case UC_FETCH_MEMORY:
        if(current_memory_step == MS_INIT) {
            // determine first step based on addressing mode
            switch(current_addressing_mode) {
            case AM_VECTOR:
                current_memory_step = MS_FETCH_VECTOR_LOW;
                break;

            case AM_IMMEDIATE_BYTE:
            case AM_IMMEDIATE_WORD:
                current_memory_step = MS_FETCH_OPERAND_LOW;
                break;

            case AM_ACCUMULATOR:
                // accumulator is immediately available
                cout << "[cpu65c816] fetching A" << endl;
                memory_byte = registers.a;
                current_memory_step = MS_DONE;
                break;
            }
            break;
        }
        break;

    case UC_FETCH_OPCODE:
        break;

    case UC_STORE_MEMORY:
        break;
    }

    // technically the ALU pins would be set up on the selected opcode here
    // and latched at the start of the next clock cycle. but we'll just implement
    // ALU ops in FinishInstructionCycle().

}

void CPU65C816::ClockRisingEdge()
{
    cout << "[cpu65c816] CPU step HIGH" << endl;
}

void CPU65C816::SetupPinsLowCycle()
{
    switch(current_uc_opcode & 0x03) {
    case UC_FETCH_OPCODE:
        cout << "[cpu65c816] asserting opcode fetch lines" << endl;
        pins.vda.AssertHigh();           // vda and vpa high means op-code fetch
        pins.vpa.AssertHigh();           // ..
        pins.rw_n.AssertHigh();          // assert read

        // do data and address after VDA/VPA/RWn
        pins.db.Assert(registers.pbr);   // opcode fetch uses program bank

        // put the PC address on the address lines 
        pins.a.Assert(registers.pc);
        break;

    case UC_FETCH_MEMORY:
        switch(current_memory_step) {
        case MS_FETCH_OPERAND_LOW:
        case MS_FETCH_OPERAND_HIGH:
        case MS_FETCH_OPERAND_BANK:
            cout << "[cpu65c816] asserting memory fetch lines for ";
            cout << "instruction operand " << (current_memory_step-MS_FETCH_OPERAND_LOW+1) << endl;
            pins.vpa.AssertHigh(); // assert VDA
            data_fetch_bank = registers.pbr;
            data_fetch_address = registers.pc;  // PC is incremented in FinishInstructionCycle() on memory fetches
            break;

        case MS_FETCH_VECTOR_LOW:
        case MS_FETCH_VECTOR_HIGH:
            cout << "[cpu65c816] asserting memory fetch lines for ";
            cout << "vector address " << (current_memory_step-MS_FETCH_VECTOR_LOW+1) << endl;
            pins.vpa.AssertHigh(); // assert VDA
            pins.vp_n.AssertLow(); // for vector fetch only, assert VPn low
            data_fetch_bank = 0;   // vector is always in bank 0
            data_fetch_address = vector_address; // use the vector address
            break;
        }

        pins.rw_n.AssertHigh(); // UC_FETCH_MEMORY is always a read

        // put bank and address on the lines after after VDA/VPA/RWn
        pins.db.Assert(data_fetch_bank);
        pins.a.Assert(data_fetch_address);

        break;

    case UC_STORE_MEMORY:
        break;
    }
}

void CPU65C816::SetupPinsHighCycle()
{
    // on a write cycle, we need to change the data bus to output the value
    if(IsWriteCycle()) {
    } else { // on every other cycle, even if it's not a read/write operation, we high-z the data bus
        pins.db.HighZ();
    }
}

