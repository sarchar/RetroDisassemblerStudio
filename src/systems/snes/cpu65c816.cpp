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
    CPU65C816::NOP_UC[]           = {                   UC_NOP                  , UC_FETCH_OPCODE };

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
    DEAD_INSTRUCTION, LDA_UC          , DEAD_INSTRUCTION, DEAD_INSTRUCTION, 
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
    DEAD_INSTRUCTION, DEAD_INSTRUCTION, INC_UC          , DEAD_INSTRUCTION, 
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
    AM_IMPLIED       , AM_DIRECT_PAGE   , AM_IMPLIED    , AM_IMPLIED,
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
    AM_IMPLIED       , AM_IMPLIED       , AM_DIRECT_PAGE, AM_IMPLIED,
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
    current_uc_opcode       = UC_NOP; // this makes the next FinishInstructionCycle() do nothing
    current_uc_set          = &CPU65C816::JMP_UC[0];
    current_uc_set_pc       = 0;
    current_addressing_mode = AM_VECTOR;
    current_memory_step     = MS_MODIFY;
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
    if(current_memory_step < MS_MODIFY) {
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
                current_memory_step = MS_MODIFY; // store the operand if needed
                break;

            case MS_FETCH_OPERAND_LOW:
                // latch the operand low byte
                memory_word = (memory_word & 0xFF00) | data_line;

                // increase PC as that's where OPERAND_LOW comes from
                registers.pc += 1;

                // move onto next stage, which might be the high byte of a word
                // or something more complex
                switch(current_addressing_mode) {
                case AM_IMMEDIATE_WORD:
                    // read the LOW byte and want a word, so stay in memory fetch
                    current_memory_step = MS_FETCH_OPERAND_HIGH;
                    current_uc_set_pc--; 
                    break;

                case AM_DIRECT_PAGE:
                    // first clear the high byte of the memory_word
                    memory_word = (memory_word & 0x00FF); 

                    // we only needed one byte, so now we add direct page to it, but only if it's nonzero
                    if(registers.d) {
                        current_memory_step = MS_ADD_D_REGISTER;
                    } else {
                        data_fetch_bank = 0; // direct page is always in bank 0
                        data_fetch_address = memory_word;
                        current_memory_step = MS_FETCH_MEMORY_LOW;
                    }
                    current_uc_set_pc--;
                    break;

                default:
                    // all other cases are done
                    current_memory_step = MS_MODIFY; // so we can store the operand if requested
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
                    current_memory_step = MS_MODIFY; // so we can store the operand if requested
                    break;
                }
                break;

            case MS_FETCH_MEMORY_LOW:
                // latch the memory low byte
                memory_byte = data_line;

                // TODO move onto next byte, if requires
                current_memory_step = MS_MODIFY; // so we can store the operand if requested
                break;

            case MS_ADD_D_REGISTER:
                memory_word += registers.d;

                // with memory_word now containing the direct page address, we need to go read the
                // actual value
                data_fetch_bank = 0;
                data_fetch_address = memory_word;
                current_memory_step = MS_FETCH_MEMORY_LOW;
                current_uc_set_pc--;
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
    } 

    if(current_memory_step == MS_MODIFY) { // if memory is done, we can execute the heart of the opcode
        // do the opcode
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

        // now that the opcode is completed, determine what to do next
        // check if we need to do a memory write, otherwise, latch whatever we can and then
        // move onto the next instruction
        switch(current_uc_opcode & 0x38) {
        case UC_STORE_MEMORY:
            switch(current_addressing_mode) {
            case AM_ACCUMULATOR:
                // accumulator is easy
                cout << "[cpu65c816] storing byte into A (AM_ACCUMULATOR)" << endl;
                registers.a = memory_byte;
                current_memory_step = MS_INIT; // reset memory step
                break;

            case AM_DIRECT_PAGE:
                // we need to stay in this uC instruction and let a data write happen
                current_memory_step = MS_WRITE_MEMORY_LOW;
                current_uc_set_pc--;
                break;

            default:
                assert(false); //unimplmeneted addressing mode for UC_STORE_MEMORY
                break;
            }
            break;

        case UC_STORE_PC:
            cout << "[cpu65c816] storing word immediate into PC" << endl;
            registers.pc = memory_word;
            current_memory_step = MS_INIT; // reset memory step
            break;

        case UC_STORE_A:
            cout << "[cpu65c816] storing byte into A" << endl;
            registers.a = memory_byte;
            current_memory_step = MS_INIT; // reset memory step
            break;

        default:
            current_memory_step = MS_INIT; // reset memory step
            break;
        }
    } else if(current_memory_step > MS_MODIFY) {
        // must be in a write mode, so a write just finished
        // determine if we need to write more bytes or be finished
        assert((current_uc_opcode & 0x38) == UC_STORE_MEMORY);
        switch(current_memory_step) {
        case MS_WRITE_MEMORY_LOW:
            // TODO need 16-bit writes
            current_memory_step = MS_INIT;
            break;
        }
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
            case AM_DIRECT_PAGE:
                current_memory_step = MS_FETCH_OPERAND_LOW;
                break;

            case AM_ACCUMULATOR:
                // accumulator is immediately available
                cout << "[cpu65c816] fetching A" << endl;
                memory_byte = registers.a;
                current_memory_step = MS_MODIFY;
                break;
            }
            break;
        }
        break;

    case UC_FETCH_OPCODE:
        break;

    // in the case there is no fetch, we need to put the memory mode into READY
    // to complete opcodes that don't have US_FETCH_*
    default:
        if(current_memory_step == MS_INIT) current_memory_step = MS_MODIFY;
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
    u16 address_offset = 0;

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
        return;

    case UC_FETCH_MEMORY:
        if(current_memory_step < MS_MODIFY) {
            switch(current_memory_step) {
            case MS_FETCH_OPERAND_LOW:
            case MS_FETCH_OPERAND_HIGH:
            case MS_FETCH_OPERAND_BANK:
                cout << "[cpu65c816] asserting memory fetch lines for ";
                cout << "instruction operand " << (current_memory_step-MS_FETCH_OPERAND_LOW+1) << endl;
                pins.vpa.AssertHigh(); // assert VPA
                data_fetch_bank = registers.pbr;
                data_fetch_address = registers.pc;  // PC is incremented in FinishInstructionCycle() on memory fetches
                break;

            case MS_FETCH_VECTOR_LOW:
            case MS_FETCH_VECTOR_HIGH:
                cout << "[cpu65c816] asserting memory fetch lines for ";
                cout << "vector address " << (current_memory_step-MS_FETCH_VECTOR_LOW+1) << endl;
                pins.vpa.AssertHigh(); // assert VPA
                pins.vp_n.AssertLow(); // for vector fetch only, assert VPn low
                data_fetch_bank = 0;   // vector is always in bank 0
                data_fetch_address = vector_address; // use the vector address
                break;

            case MS_FETCH_MEMORY_LOW:
                cout << "[cpu65c816] asserting memory fetch lines for ";
                cout << "memory address " << (current_memory_step-MS_FETCH_MEMORY_LOW+1) << endl;
                pins.vda.AssertHigh(); // assert VDA
                // data_fetch_address and bank are already set up but we'll need an offset for low/high/bank bytes
                // the data_fetch_address can't change since it may be used for both read and write like in INC $00.
                address_offset = (current_memory_step - MS_FETCH_MEMORY_LOW);
                break;
            }

            pins.rw_n.AssertHigh(); // UC_FETCH_MEMORY is always a read

            // put bank and address on the lines after after VDA/VPA/RWn
            pins.db.Assert(data_fetch_bank);
            pins.a.Assert(data_fetch_address + address_offset);

            return;
        } else break;
    }

    switch(current_uc_opcode & 0x3C) {
    case UC_STORE_MEMORY:
        switch(current_memory_step) {
        case MS_WRITE_MEMORY_LOW:
            cout << "[cpu65c816] asserting memory write lines for ";
            cout << "memory address " << (current_memory_step-MS_WRITE_MEMORY_LOW+1) << endl;
            pins.vda.AssertHigh(); // assert VDA
            // data_fetch_address and bank are already set up but we'll need an offset for low/high/bank bytes
            // the data_fetch_address can't change since it may be used for both read and write like in INC $00.
            address_offset = (current_memory_step - MS_WRITE_MEMORY_LOW);
            break;
        }

        pins.rw_n.AssertLow(); // UC_STORE_MEMORY is always a write

        // put bank and address on the lines after after VDA/VPA/RWn
        pins.db.Assert(data_fetch_bank);
        pins.a.Assert(data_fetch_address + address_offset);

        return;
    }
}

void CPU65C816::SetupPinsHighCycle()
{
    // on a write cycle, we need to change the data bus to output the value
    if(IsWriteCycle()) {
        pins.db.Assert(memory_byte);
    } else { // on every other cycle, even if it's not a read/write operation, we high-z the data bus
        pins.db.HighZ();
    }
}

