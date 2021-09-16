#include <functional>
#include <iostream>
#include <iomanip>

#include "systems/snes/cpu65c816.h"

#include "systems/snes/cpu65c816_instructions.inc"

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

    registers.d      = 0x1234; //0;
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
        switch(current_uc_opcode & UC_FETCH_MASK) {
        case UC_FETCH_MEMORY:
            switch(current_memory_step) {
            case MS_FETCH_VECTOR_LOW:
                // latch the vector low byte
                intermediate_data.as_byte = (intermediate_data.as_byte & 0xFF00) | data_line;

                // move to the next vector byte
                vector_address += 1;

                // get the high byte and keep running UC_FETCH_MEMORY
                current_memory_step = MS_FETCH_VECTOR_HIGH;
                current_uc_set_pc--;
                break;

            case MS_FETCH_VECTOR_HIGH:
                // latch the vector high byte
                intermediate_data.high_byte = data_line;

                // finished a vector pull
                vector_pull = false;

                // we're done with memory, so we can move on and store the operand
                current_memory_step = MS_MODIFY;
                break;

            case MS_FETCH_OPERAND_LOW:
                // latch the operand low byte
                intermediate_data.as_byte = data_line;
                intermediate_data_size = 1; // default to byte size, might be increased later

                // increase PC as that's where OPERAND_LOW comes from
                registers.pc += 1;

                // move onto next stage, which might be the high byte of a word or something more complex
                // like direct page or indexed modes
                switch(current_addressing_mode) {
                case AM_IMMEDIATE_WORD:
                    // we've read the low byte and want a word, so stay in operand fetch
                    current_memory_step = MS_FETCH_OPERAND_HIGH;
                    current_uc_set_pc--; 
                    break;

                case AM_DIRECT_PAGE:
                    // first clear the high byte of the intermediate data
                    intermediate_data.high_byte = 0;

                    // we only needed one byte but we need to add direct page to it
                    // if it's 0, we can skip the add saving 1 clcok cycle
                    if(registers.d) {
                        current_memory_step = MS_ADD_D_REGISTER;
                    } else {
                        // set up the memory fetch
                        data_rw_bank        = 0; // direct page is always in bank 0
                        data_rw_address     = intermediate_data.as_word;
                        current_memory_step = MS_FETCH_MEMORY_LOW;
                    }

                    // in both cases we still stay in the current uC step
                    current_uc_set_pc--;
                    break;

                default:
                    // all other cases are done, process the data and/or store it as necessary
                    current_memory_step = MS_MODIFY;
                    break;
                }
                break;

            case MS_FETCH_OPERAND_HIGH:
                // latch the operand high byte
                intermediate_data.high_byte = data_line;

                // increase PC to the next operand byte or instruction
                registers.pc += 1;

                // TODO move onto bank byte if necessasry
                switch(current_addressing_mode) {
                //!case AM_IMMEDIATE_LONG:
                //!    // read the LOW byte and want a word, so stay in memory fetch
                //!    current_memory_step = MS_FETCH_OPERAND_BANK;
                //!    current_uc_set_pc--; 
                //!    break;

                default:
                    // all other cases are done, process the data and/or store it as necessary
                    current_memory_step = MS_MODIFY;
                    break;
                }
                break;

            case MS_FETCH_MEMORY_LOW:
                // latch the memory low byte
                intermediate_data.as_byte = data_line;

                // TODO move onto next byte, if required
                // TODO how to tell if fetch is for memory or index?
                //!if(IsWordMemoryEnabled()) {
                //!}

                // we're done with memory, so we can move on and store the operand
                current_memory_step = MS_MODIFY;
                break;

            case MS_ADD_D_REGISTER:
                intermediate_data.as_word += registers.d;

                // with intermediate_data now containing the direct page address, we need to go read the value from memory
                data_rw_bank        = 0; // direct page is always zero
                data_rw_address     = intermediate_data.as_word;
                current_memory_step = MS_FETCH_MEMORY_LOW;
                current_uc_set_pc--;     // stay on the same uC instruction
                break;
            }

            break;

        // Fetching an opcode is always one byte, and it's separate from UC_FETCH_MEMORY
        // because it also "decodes" the opcode and resets the uC pointer.
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

            // reeet memroy mode
            current_memory_step = MS_INIT;
            break;
        }
    } /* current_memory_step < MS_MODIFY */

    // we fall through from the above if statement so we can process the read value from memory
    // in the cycle it becomes available. however, writing data back to memory has to wait a full write cycle

    if(current_memory_step == MS_MODIFY) { // if memory is done, we can execute the heart of the opcode
        // do the opcode
        switch(current_uc_opcode & UC_OPCODE_MASK) {
        case UC_DEAD:
            // prevent the instruction from moving on by keeping the microcode fixed at 0
            current_uc_set_pc = 0;
            break;

        case UC_NOP:
            break;

        case UC_INC:
            // always increment the word value
            intermediate_data.as_word += 1;
            break;
        }

        // TODO set flags here based on the result of the operation

        // now that the opcode is completed, determine what to do with the result, if anything
        // check if we need to do a memory write or store it in a register
        // for memory writes, we need extra clock cycles. for registers, we can store the
        // result immediately and move onto the next cycle
        switch(current_uc_opcode & UC_STORE_MASK) {
        case UC_STORE_MEMORY:
            switch(current_addressing_mode) {
            case AM_ACCUMULATOR:
                // accumulator is easy
                cout << "[cpu65c816] storing byte into A (AM_ACCUMULATOR)" << endl;

                // TODO
                //!if(IsWordMemoryEnabled()) {
                //!    registers.a = intermediate_data.as_word;
                //!} else {
                    registers.a = intermediate_data.as_byte;
                //!}

                // move onto the next uC opcode
                current_memory_step = MS_INIT;
                break;

            case AM_DIRECT_PAGE:
                // for direct page, the memory address has already been computed and stored in data_rw_address/bank
                // stay in this uC instruction and write the data to the address it came from
                current_memory_step = MS_WRITE_MEMORY_LOW;
                current_uc_set_pc--;
                break;

            case AM_STACK:
                // UC_STORE_MEMORY with addressing mode AM_STACK implies a push
                // TODO determine size of memory and push HIGH first, since stack bytes are written in reverse order
                if(intermediate_data_size == 2) {
                    current_memory_step = MS_WRITE_STACK_HIGH;
                } else {
                    current_memory_step = MS_WRITE_STACK_LOW;
                }
                current_uc_set_pc--;

                // set the memory address to write to the stack
                data_rw_bank    = 0; // stack always in bank 0
                data_rw_address = registers.s;

                // post-decrement stack pointer
                registers.s    -= 1;
                break;

            default:
                assert(false); //unimplmeneted addressing mode for UC_STORE_MEMORY
                break;
            }
            break;

        case UC_STORE_PC:
            cout << "[cpu65c816] storing word immediate into PC" << endl;
            // store the 16-bit PC
            registers.pc = intermediate_data.as_word;

            // move onto the next uC opcode
            current_memory_step = MS_INIT;
            break;

        case UC_STORE_A:
            cout << "[cpu65c816] storing byte into A" << endl;
            // TODO determine size of A
            //!if(IsWordMemoryEnabled()) {
            //!    registers.c = intermediate_data.as_word;
            //!} else {
                registers.a = intermediate_data.as_byte;
            //!}

            // move onto the next uC opcode
            current_memory_step = MS_INIT;
            break;

        case UC_STORE_NONE:
            // move onto the next uC opcode
            current_memory_step = MS_INIT;
            break;

        default:
            assert(false); // unimplemented UC_STORE* operation
            break;
        }
    } else if(current_memory_step > MS_MODIFY) {
        // if we arrive here on a following cycle after MS_MODIFY, then we must have just
        // issued a data write. now, determine if we need to continue writing more data
        assert((current_uc_opcode & UC_STORE_MASK) == UC_STORE_MEMORY);

        switch(current_memory_step) {
        case MS_WRITE_MEMORY_LOW:
            // TODO need 16-bit writes
            current_memory_step = MS_INIT;
            break;

        case MS_WRITE_STACK_LOW:
            // TODO need 16 and 24-bit writes
            current_memory_step = MS_INIT;
            break;

        case MS_WRITE_STACK_HIGH:
            // a STACK_HIGH byte is always followed by a STACK_LOW byte
            current_memory_step = MS_WRITE_STACK_LOW;
            
            // set the memory address to the next stack address
            data_rw_bank    = 0; // stack always in bank 0
            data_rw_address = registers.s;

            // post-decrement stack pointer
            registers.s    -= 1;

            current_uc_set_pc--;
            break;
        }
    }
}

void CPU65C816::StartInstructionCycle()
{
    if(current_memory_step == MS_INIT) {
        switch(current_uc_opcode & UC_FETCH_MASK) {
        case UC_FETCH_MEMORY:
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
                // TODO get rid of me
                cout << "[cpu65c816] fetching A" << endl;
                //!if(IsWordMemoryEnabled()) {
                //!    intermediate_data.as_word = registers.c;
                //!    intermediate_data_size = 2;
                //!} else {
                    intermediate_data.as_byte = registers.a;
                    intermediate_data_size = 1;
                //!}

                // skip any memory fetch and immediately perform the operation
                current_memory_step = MS_MODIFY;
                break;
            }
            break;

        case UC_FETCH_OPCODE:
            break;

        case UC_FETCH_D:
            cout << "[cpu65c816] fetching D" << endl;
            intermediate_data.as_word = registers.d;
            intermediate_data_size = 2;

            // skip any memory fetch and immediately perform the operation
            current_memory_step = MS_MODIFY;
            break;

        // in the case there is no fetch, we need to put the memory mode into MODIFY
        // to complete opcodes that don't require a US_FETCH_* (ie., CLC)
        default:
            if(current_memory_step == MS_INIT) current_memory_step = MS_MODIFY;
            break;
        }
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
    if(current_memory_step < MS_MODIFY) {
        // If we're in a FETCH cycle, set up the pins accordingly
        SetupPinsLowCycleForFetch();
    } else if(current_memory_step > MS_MODIFY) {
        // If we're in a STORE cycle, set up the pins accordingly
        SetupPinsLowCycleForStore();
    }
}

void CPU65C816::SetupPinsLowCycleForFetch()
{
    u16 address_offset = 0;

    switch(current_uc_opcode & UC_FETCH_MASK) {
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
        cout << "[cpu65c816] asserting memory fetch lines for ";

        switch(current_memory_step) {
        case MS_FETCH_OPERAND_LOW:
        case MS_FETCH_OPERAND_HIGH:
        case MS_FETCH_OPERAND_BANK:
            cout << "instruction operand byte " << (current_memory_step - MS_FETCH_OPERAND_LOW) << endl;
            pins.vpa.AssertHigh(); // assert VPA
            data_rw_bank    = registers.pbr; // operands use the program bank
            data_rw_address = registers.pc;  // PC is incremented in FinishInstructionCycle() on memory fetches
            break;
        
        case MS_FETCH_VECTOR_LOW:
        case MS_FETCH_VECTOR_HIGH:
            cout << "vector address byte " << (current_memory_step - MS_FETCH_VECTOR_LOW) << endl;
            pins.vpa.AssertHigh(); // assert VPA
            pins.vp_n.AssertLow(); // for vector fetch only, assert VPn low
            data_rw_bank = 0;      // vector is always in bank 0
            data_rw_address = vector_address; // use the vector address
            break;
        
        case MS_FETCH_MEMORY_LOW:
            cout << "memory address byte " << (current_memory_step - MS_FETCH_MEMORY_LOW) << endl;
            pins.vda.AssertHigh(); // assert VDA
            // data_rw_address and bank are already set up but we'll need an offset for low/high/bank bytes
            // the data_rw_address can't change since it may be used for both read and write like in INC $00.
            address_offset = (current_memory_step - MS_FETCH_MEMORY_LOW);
            break;

        case MS_ADD_D_REGISTER:
            cout << "adding D register to intermediate address (no fetch)" << endl;
            break;
        }
        
        pins.rw_n.AssertHigh(); // UC_FETCH_MEMORY is always a read
        
        // put bank and address on the lines after after VDA/VPA/RWn
        pins.db.Assert(data_rw_bank);
        pins.a.Assert(data_rw_address + address_offset);
        
        break;
    }
}

void CPU65C816::SetupPinsLowCycleForStore()
{
    u16 address_offset = 0;

    switch(current_uc_opcode & UC_STORE_MASK) {
    case UC_STORE_MEMORY:
        cout << "[cpu65c816] asserting memory store lines for ";

        switch(current_memory_step) {
        case MS_WRITE_MEMORY_LOW:
            cout << "memory address byte " << (current_memory_step - MS_WRITE_MEMORY_LOW) << endl;
            pins.vda.AssertHigh(); // assert VDA
            // data_rw_address and bank are already set up but we'll need an offset for low/high/bank bytes
            // the data_rw_address can't change since it may be used for both read and write like in INC $00.
            address_offset = (current_memory_step - MS_WRITE_MEMORY_LOW);
            // write the low byte
            data_w_value = intermediate_data.as_byte;
            break;

        case MS_WRITE_STACK_LOW:
        case MS_WRITE_STACK_HIGH:
            cout << "stack address byte " << (current_memory_step - MS_WRITE_STACK_LOW) << endl;
            pins.vda.AssertHigh(); // assert VDA
            // the stack register will change, so we don't need an offset
            if(current_memory_step == MS_WRITE_STACK_HIGH) {
                data_w_value = intermediate_data.high_byte;
            } else {
                data_w_value = intermediate_data.as_byte;
            }
            break;
        }

        pins.rw_n.AssertLow(); // UC_STORE_MEMORY is always a write

        // put bank and address on the lines after after VDA/VPA/RWn
        pins.db.Assert(data_rw_bank);
        pins.a.Assert(data_rw_address + address_offset);

        return;
    }
}

void CPU65C816::SetupPinsHighCycle()
{
    // on a write cycle, we need to change the data bus to output the value
    if(IsWriteCycle()) {
        pins.db.Assert(data_w_value);
    } else { // on every other cycle, even if it's not a read/write operation, we high-z the data bus
        pins.db.HighZ();
    }
}

