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
    current_uc_opcode         = UC_NOP; // this makes the next FinishInstructionCycle() do nothing
    current_uc_set            = &CPU65C816::JMP_UC[0];
    current_uc_set_pc         = 0;
    current_addressing_mode   = AM_VECTOR;
    current_memory_step       = MS_MODIFY;
    operand_address.bank_byte = 0;
    operand_address.as_word   = 0xFFFC;
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
        // for instructions that require a computed memory address,
        // finish the current step and perform the next step
        if(((current_uc_opcode & UC_FETCH_MASK) == UC_FETCH_MEMORY)
            || (current_uc_opcode & UC_STORE_MASK) == UC_STORE_MEMORY) {
            bool is_fetch = ((current_uc_opcode & UC_FETCH_MASK) == UC_FETCH_MEMORY);
            StepMemoryAccessCycle(is_fetch, data_line);
        } else {
            // handle various other cases that don't make use of the addressing mode
            switch(current_uc_opcode & UC_FETCH_MASK) {
            // Fetching an opcode is always one byte, and it's separate from UC_FETCH_MEMORY
            // because it also "decodes" the opcode and resets the uC pointer.
            case UC_FETCH_OPCODE:
                // store opcode in intermediate data
                intermediate_data.as_byte = data_line;

                // increment the program counter
                registers.pc += 1;

                // move on to decode the value
                current_memory_step = MS_MODIFY;
                break;
            }
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

        case UC_DEC:
            // decrement the value
            intermediate_data.as_byte -= 1;
            break;

        case UC_INC:
            // increment the value
            intermediate_data.as_byte += 1;
            break;

        case UC_EOR:
            // eXclusive OR A with memory
            intermediate_data.as_byte ^= registers.a;
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
            case AM_DIRECT_PAGE:
                // for direct page, the memory address has already been computed and stored in data_rw_address/bank
                // stay in this uC instruction and write the data to the address it came from
                current_memory_step = MS_WRITE_VALUE_LOW;
                current_uc_set_pc--;
                break;

            case AM_STACK:
                // stack write operations have an extra IO cycle before the actual write (that's the previous
                // cycle that's finishing right now).  Now we move onto the first stack write

                // UC_STORE_MEMORY with addressing mode AM_STACK implies a push
                // TODO determine size of memory and push HIGH first, since stack bytes are written in reverse order
                if(intermediate_data_size == 2) {
                    current_memory_step = MS_WRITE_STACK_HIGH;
                } else {
                    current_memory_step = MS_WRITE_STACK_LOW;
                }
                current_uc_set_pc--;

                // set the memory address to write to the stack
                operand_address.bank_byte = 0; // stack always in bank 0
                operand_address.as_word = registers.s;

                // post-decrement stack pointer
                registers.sl -= 1;
                break;

            default:
                assert(false); //unimplmeneted addressing mode for UC_STORE_MEMORY
                break;
            }
            break;

        case UC_STORE_IR:
            cout << "[cpu65c816] storing intermediate byte into IR" << endl;

            // IR could be a Bus<u8>, and when set, triggers the instruction decode
            // but that would be overkill....like this entire project!
            registers.ir = intermediate_data.as_byte;

            // determine addressing mode
            current_addressing_mode = CPU65C816::INSTRUCTION_ADDRESSING_MODES[registers.ir];

            // select microcode and reset counter
            current_uc_set = &CPU65C816::INSTRUCTION_UCs[registers.ir][0];
            current_uc_set_pc = 0;

            // reset memory step
            current_memory_step = MS_INIT;
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

        case UC_STORE_X:
            cout << "[cpu65c816] storing byte into X" << endl;
            // TODO determine size of A
            //!if(IsWordMemoryEnabled()) {
            //!    registers.x = intermediate_data.as_word;
            //!} else {
                registers.xl = intermediate_data.as_byte;
            //!}

            // move onto the next uC opcode
            current_memory_step = MS_INIT;
            break;

        case UC_STORE_S:
            cout << "[cpu65c816] storing byte into S" << endl;
            // TODO determine size of A
            //!if(IsWordMemoryEnabled()) {
            //!    registers.x = intermediate_data.as_word;
            //!} else {
                registers.sl = intermediate_data.as_byte;
                registers.sh = 0x01;
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
        case MS_WRITE_VALUE_LOW:
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
            operand_address.as_word = registers.s;

            // post-decrement stack pointer
            registers.sl -= 1;

            current_uc_set_pc--;
            break;
        }
    }
}

// Latches whatever current data is on the data line and moves onto the next step
// in computing the memory address
void CPU65C816::StepMemoryAccessCycle(bool is_fetch, u8 data_line)
{
    switch(current_memory_step) {
    case MS_FETCH_VECTOR_LOW:
        // latch the vector low byte
        intermediate_data.as_byte = (intermediate_data.as_byte & 0xFF00) | data_line;

        // get the high byte and keep running UC_FETCH_MEMORY
        current_memory_step = MS_FETCH_VECTOR_HIGH;
        current_uc_set_pc--;
        break;

    case MS_FETCH_VECTOR_HIGH:
        // latch the vector high byte
        intermediate_data.high_byte = data_line;

        // we're done with memory, so we can move on and store the operand
        current_memory_step = MS_MODIFY;
        break;

    case MS_FETCH_OPERAND_LOW:
        // latch the operand low byte to start an address
        operand_address.as_byte = data_line;

        // increase PC as that's where OPERAND_LOW comes from
        registers.pc += 1;

        // move onto next stage, which might be the high byte of a word or something more complex
        // like direct page or indexed modes
        switch(current_addressing_mode) {
        case AM_IMMEDIATE_BYTE:
            // move the operand to data since it's used as data
            intermediate_data.as_byte = data_line;
            intermediate_data_size = 1; // default to byte size, might be increased later

            // for immediate bytes, we 're done
            current_memory_step = MS_MODIFY;
            break;

        case AM_IMMEDIATE_WORD:
            // move the operand to data since it's used as data
            intermediate_data.as_byte = data_line;

            // we've read the low byte and want a word, so stay in operand fetch, but fetch the high byte
            current_memory_step = MS_FETCH_OPERAND_HIGH;
            current_uc_set_pc--; 
            break;

        case AM_DIRECT_PAGE:
            // first clear the high byte of the data address
            operand_address.high_byte = 0;

            // and direct page is always in bank 0
            operand_address.bank_byte = 0;

            // we only needed one byte but we need to add direct page to it
            // if it's 0, we can skip the add, saving 1 clcok cycle
            if(registers.d) {
                current_memory_step = MS_ADD_D_REGISTER;
                current_uc_set_pc--;
            } else {
                // if it's a fetch, go fetch the data
                if(is_fetch) {
                    current_memory_step = MS_FETCH_VALUE_LOW;
                    current_uc_set_pc--;
                } else {
                    // otherwise, continue on
                    current_memory_step = MS_MODIFY;
                }
            }

            break;

        default:
            // all other cases are done, process the data and/or store it as necessary
            current_memory_step = MS_MODIFY;
            break;
        }
        break;

    case MS_FETCH_OPERAND_HIGH:
        // latch the operand high byte
        operand_address.high_byte = data_line;

        // increase PC to the next operand byte or instruction
        registers.pc += 1;

        // TODO move onto bank byte if necessasry
        switch(current_addressing_mode) {
        case AM_IMMEDIATE_WORD:
            // move the operand to data
            intermediate_data.high_byte = data_line;

            // wanted a word, and now we have a word, so done
            current_memory_step = MS_MODIFY;
            break;

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

    case MS_FETCH_VALUE_LOW:
        // latch the memory low byte
        intermediate_data.as_byte = data_line;

        // TODO move onto next byte, if required
        // TODO how to tell if fetch is for memory or index?
        //!if(IsWordMemoryEnabled()) {
        //!}

        // we're done with memory, so we can move on and store the operand
        current_memory_step = MS_MODIFY;
        break;

    case MS_FETCH_STACK_LOW:
        // latch the stack low byte
        intermediate_data.as_byte = data_line;

        // determine if we need to read the high byte
        if(intermediate_data_size == 2) {
            current_memory_step = MS_FETCH_STACK_HIGH;
            current_uc_set_pc--;
        } else {
            current_memory_step = MS_MODIFY;
        }
        break;

    case MS_ADD_D_REGISTER:
        operand_address.as_word += registers.d;

        // with operand_address now containing the direct page address, we need to go read the value from memory
        // or we're finished computing the address
        if(is_fetch) {
            current_memory_step = MS_FETCH_VALUE_LOW;
            current_uc_set_pc--;     // stay on the same uC instruction
        } else {
            current_memory_step = MS_MODIFY;
        }
        break;
    }
}

void CPU65C816::StartInstructionCycle()
{
    if(current_memory_step == MS_INIT) {
        // if we have either fetch or store into memory, we need to compute
        // the memory address before we execute the instruction. the addressing
        // mode of the instruction tells us what to do
        if(((current_uc_opcode & UC_FETCH_MASK) == UC_FETCH_MEMORY)
            || (current_uc_opcode & UC_STORE_MASK) == UC_STORE_MEMORY) {

            bool is_fetch = ((current_uc_opcode & UC_FETCH_MASK) == UC_FETCH_MEMORY);

            switch(current_addressing_mode) {
                case AM_VECTOR:
                    current_memory_step = MS_FETCH_VECTOR_LOW;
                    break;

                case AM_IMMEDIATE_BYTE:
                case AM_IMMEDIATE_WORD:
                case AM_DIRECT_PAGE:
                    current_memory_step = MS_FETCH_OPERAND_LOW;
                    break;

                case AM_STACK:
                    if(is_fetch) {
                        // TODO stack fetch cycle requires two(!) IO cycles, maybe to determine how many data bytes to pull
                        // or maybe to set up S. for now we are wrong but will need to be fixed soon
                        // maybe with MS_FETCH_STACK_GET_ITEM_SIZE and MS_FETCH_STACK_SETUP ?
                        // see item 22b on page 43 in the datasheet. Stack writes are currently implemented correctly.

                        // TODO determine # of bytes to pull later?
                        switch(current_uc_opcode & UC_STORE_MASK) {
                        case UC_STORE_A:
                        case UC_STORE_X:
                        case UC_STORE_Y:
                            intermediate_data_size = 1;
                            break;
                        case UC_STORE_D:
                            intermediate_data_size = 2;
                            break;
                        default:
                            assert(false); // unknown store value for stack
                            break;
                        }

                        // increment S (TODO might be done on the IO cycle)
                        registers.sl += 1;

                        // set up the read address
                        operand_address.bank_byte = 0; // all stack operations in bank 0
                        operand_address.as_word   = registers.s;

                        // always start with the low stack byte
                        current_memory_step = MS_FETCH_STACK_LOW;
                    }
                    break;
            }
        } 

        // when we have UC_STORE_MEMORY and not UC_FETCH_MEMORY, we may need to also 
        // fetch some other register at this point
        switch(current_uc_opcode & UC_FETCH_MASK) {
        case UC_FETCH_MEMORY:
            // was handled above, so do nothing here, leave current_memory_step alone
            break;

        case UC_FETCH_OPCODE:
            // stay in MS_INIT
            break;

        case UC_FETCH_ZERO:
            cout << "[cpu65c816] fetching ZERO" << endl;
            intermediate_data.as_word = 0;
            // TODO don't need to set intermediate_data_size because nothing that uses
            // UC_FETCH_ZERO relies on it
            break;

        case UC_FETCH_A:
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

        case UC_FETCH_X:
            cout << "[cpu65c816] fetching X" << endl;
            intermediate_data.as_byte = registers.xl;
            intermediate_data_size = 1;

            // skip any memory fetch and immediately perform the operation
            current_memory_step = MS_MODIFY;
            break;

        case UC_FETCH_D:
            cout << "[cpu65c816] fetching D" << endl;
            intermediate_data.as_word = registers.d;
            intermediate_data_size = 2;

            // skip any memory fetch and immediately perform the operation
            current_memory_step = MS_MODIFY;
            break;

        case UC_FETCH_S:
            cout << "[cpu65c816] fetching S" << endl;
            intermediate_data.as_word = registers.s;
            intermediate_data_size = 2;

            // skip any memory fetch and immediately perform the operation
            current_memory_step = MS_MODIFY;
            break;

        // in the case there is no fetch, we need to put the memory mode into MODIFY
        // to complete opcodes that don't require a UC_FETCH_* (ie., CLC)
        default:
            current_memory_step = MS_MODIFY;
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
    u8  data_rw_bank;
    u16 data_rw_address;

    // if the operand address is being computed, put the correct data
    // on the pins
    if(((current_uc_opcode & UC_FETCH_MASK) == UC_FETCH_MEMORY)
        || (current_uc_opcode & UC_STORE_MASK) == UC_STORE_MEMORY) {
        cout << "[cpu65c816] asserting memory fetch lines for ";

        switch(current_memory_step) {
        case MS_FETCH_OPERAND_LOW:
        case MS_FETCH_OPERAND_HIGH:
        case MS_FETCH_OPERAND_BANK:
            cout << "instruction operand byte " << (current_memory_step - MS_FETCH_OPERAND_LOW) << endl;
            pins.vpa.AssertHigh(); // assert VPA
            data_rw_bank    = registers.pbr; // operands use the program bank
            data_rw_address = registers.pc;  // PC is incremented in FinishInstructionCycle() on operand fetches
            break;
        
        case MS_FETCH_VECTOR_LOW:
        case MS_FETCH_VECTOR_HIGH:
            cout << "vector address byte " << (current_memory_step - MS_FETCH_VECTOR_LOW) << endl;
            pins.vpa.AssertHigh(); // assert VPA
            pins.vp_n.AssertLow(); // for vector fetch only, assert VPn low
            data_rw_bank    = operand_address.bank_byte;  // vector is always in bank 0, but use it anyway
            data_rw_address = operand_address.as_word + (current_memory_step - MS_FETCH_VECTOR_LOW); // use the vector address
            break;
        
        case MS_FETCH_VALUE_LOW:
            cout << "memory address byte " << (current_memory_step - MS_FETCH_VALUE_LOW) << endl;
            pins.vda.AssertHigh(); // assert VDA
            // data_rw_address and bank are already set up but we'll need an offset for low/high/bank bytes
            // the data_rw_address can't change since it may be used for both read and write like in INC $00.
            data_rw_bank    = operand_address.bank_byte;
            data_rw_address = operand_address.as_word + (current_memory_step - MS_FETCH_VALUE_LOW);
            break;

        case MS_FETCH_STACK_LOW:
        case MS_FETCH_STACK_HIGH:
            cout << "stack byte " << (current_memory_step - MS_FETCH_STACK_LOW) << endl;
            pins.vda.AssertHigh(); // assert VDA
            // operand_address is the S register, which is incremented in FinishInstructionCycle() us as we read stack values
            data_rw_bank    = operand_address.bank_byte;
            data_rw_address = operand_address.as_word;
            break;

        case MS_ADD_D_REGISTER:
            cout << "adding D register to intermediate address (no fetch)" << endl;
            break;
        }
        
        pins.rw_n.AssertHigh(); // UC_FETCH_MEMORY is always a read
        
        // put bank and address on the lines after after VDA/VPA/RWn
        pins.db.Assert(data_rw_bank);
        pins.a.Assert(data_rw_address);
    } else {        
        // other cases use a different method of asserting the lines
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
        }
    }
}

void CPU65C816::SetupPinsLowCycleForStore()
{
    u8  data_rw_bank;
    u16 data_rw_address;

    switch(current_uc_opcode & UC_STORE_MASK) {
    case UC_STORE_MEMORY:
        cout << "[cpu65c816] asserting memory store lines for ";

        switch(current_memory_step) {
        case MS_WRITE_VALUE_LOW:
            cout << "memory address byte " << (current_memory_step - MS_WRITE_VALUE_LOW) << endl;
            pins.vda.AssertHigh(); // assert VDA

            // setup the low byte write address and value
            data_w_value    = intermediate_data.as_byte;
            data_rw_bank    = operand_address.bank_byte;

            // add offset for the high and bank bytes
            data_rw_address = operand_address.as_word + (current_memory_step - MS_WRITE_VALUE_LOW);
            break;

        case MS_WRITE_STACK_HIGH:
        case MS_WRITE_STACK_LOW:
            cout << "stack address byte " << (current_memory_step - MS_WRITE_STACK_HIGH) << endl;
            pins.vda.AssertHigh(); // assert VDA

            // the stack register will change, so we don't need an offset
            if(current_memory_step == MS_WRITE_STACK_HIGH) {
                data_w_value = intermediate_data.high_byte;
            } else {
                data_w_value = intermediate_data.as_byte;
            }

            data_rw_bank    = operand_address.bank_byte;
            data_rw_address = operand_address.as_word;
            break;
        }

        pins.rw_n.AssertLow(); // UC_STORE_MEMORY is always a write

        // put bank and address on the lines after after VDA/VPA/RWn
        pins.db.Assert(data_rw_bank);
        pins.a.Assert(data_rw_address);

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

