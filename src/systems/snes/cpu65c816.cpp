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
    if(current_memory_step < MS_MODIFY_WAIT) {
        // for instructions that require a computed memory address,
        // finish the current step and perform the next step
        bool is_memory_fetch = ((current_uc_opcode & UC_FETCH_MASK) == UC_FETCH_MEMORY);
        bool is_memory_store = ((current_uc_opcode & UC_STORE_MASK) == UC_STORE_MEMORY);
        if(is_memory_fetch || is_memory_store) {
            StepMemoryAccessCycle(is_memory_fetch, is_memory_store, data_line);
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
    }

    // we fall through from the above if statement so we can process the read value from memory
    // in the cycle it becomes available. however, writing data back to memory has to wait a full write cycle

    if(current_memory_step == MS_MODIFY_WAIT) {
        // supposed to be the IO operation here, but we'll do it right before the store
        current_memory_step = MS_MODIFY;
        current_uc_set_pc--;
    } else if(current_memory_step == MS_MODIFY) { // if memory is done, we can execute the heart of the opcode
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

        case UC_ORA:
            // OR A with memory
            intermediate_data.as_byte |= registers.a;
            break;

        // case UC_XCE:
        // TODO When switching from emulation to native mode the processor replaces the B BREAK flag 
        // and bit 5 with the 65816 M and X flags, and sets them to one. 
        // TODO you lose the high byte of the stack pointer when you switch from native to emulation
        // TODO X and Y register high bytes are lost
        // TODO A high byte is not lost (it's in B register)
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
            case AM_DIRECT_INDEXED_X:
            case AM_DIRECT_INDEXED_Y:
            case AM_DIRECT_INDIRECT:
            case AM_DIRECT_INDEXED_X_INDIRECT:
            case AM_DIRECT_INDIRECT_INDEXED_Y:
            case AM_ABSOLUTE:
            case AM_ABSOLUTE_INDEXED_X:
            case AM_ABSOLUTE_INDEXED_Y:
                // for these modes, the memory address has already been computed and stored in operand_address
                // stay in this uC instruction and write the data to the address it came from
                // TODO the value needs to be written in reverse (high byte first) if it's a R-M-W instruction
                current_memory_step = MS_WRITE_VALUE_LOW;
                current_uc_set_pc--;
                break;

            // absolute indirect modes don't have any instructions that store to memory
            case AM_ABSOLUTE_INDEXED_X_INDIRECT:
            case AM_ABSOLUTE_INDIRECT:
                assert(false);
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

        case UC_STORE_Y:
            cout << "[cpu65c816] storing byte into Y" << endl;
            // TODO determine size of A
            //!if(IsWordMemoryEnabled()) {
            //!    registers.x = intermediate_data.as_word;
            //!} else {
                registers.yl = intermediate_data.as_byte;
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

        case MS_WRITE_VALUE_HIGH:
            assert(false);
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
void CPU65C816::StepMemoryAccessCycle(bool is_memory_fetch, bool is_memory_store, u8 data_line)
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
        if(ShouldFetchOperandHigh()) {
            current_memory_step = MS_FETCH_OPERAND_HIGH;
            current_uc_set_pc--;
        } else {
            SetMemoryStepAfterOperandFetch(is_memory_fetch);
        }
        break;

    case MS_FETCH_OPERAND_HIGH:
        // latch the operand high byte
        operand_address.high_byte = data_line;

        // increase PC to the next operand byte or instruction
        registers.pc += 1;

        // move onto next stage, which might be the high byte of a word or something more complex
        // like direct page or indexed modes
        if(ShouldFetchOperandBank()) {
            current_memory_step = MS_FETCH_OPERAND_BANK;
            current_uc_set_pc--;
        } else {
            SetMemoryStepAfterOperandFetch(is_memory_fetch);
        }
        break;

    case MS_FETCH_OPERAND_BANK:
        assert(false); //TODO
        break;

    case MS_FETCH_INDIRECT_LOW:
        // set the bank byte to the data bank
        indirect_address.bank_byte = registers.dbr;

        // latch the indirect address low byte
        indirect_address.as_byte = data_line;

        // TODO page wrap appropriately
        operand_address.as_word += 1;

        // indirect addresses are always at least word size, so go read the high byte
        current_memory_step = MS_FETCH_INDIRECT_HIGH;
        current_uc_set_pc--;
        break;

    case MS_FETCH_INDIRECT_HIGH:
        // latch the indirect address high byte
        indirect_address.high_byte = data_line;

        if(ShouldFetchIndirectBank()) {
            current_memory_step = MS_FETCH_INDIRECT_BANK;
            current_uc_set_pc--;
        } else {
            // now we have an indirect_address, overwrite operand_address with it and either fetch data or move on
            operand_address = indirect_address;

            // might need to do more processing depending on the addressing mode
            SetMemoryStepAfterIndirectAddressFetch(is_memory_fetch);
        }
        break;

    case MS_FETCH_INDIRECT_BANK:
        assert(false); //unimplemented
        break;

    case MS_FETCH_VALUE_LOW:
        // latch the memory low byte
        intermediate_data.as_byte = data_line;

        // start the data size read at 1
        intermediate_data_size = 1;

        if(ShouldFetchValueHigh()) {
            current_memory_step = MS_FETCH_VALUE_HIGH;
            current_uc_set_pc--;
        } else {
            // we're done with memory, but for R-W-M instructions (instructions where we fetch and write the same
            // memory), we have to simulate an extra
            if(is_memory_store) {
                current_memory_step = MS_MODIFY_WAIT;
            } else {
                current_memory_step = MS_MODIFY;
            }
        }
        break;

    case MS_FETCH_VALUE_HIGH:
        // latch the memory high byte
        intermediate_data.high_byte = data_line;

        // increment the data size read
        intermediate_data_size++;

        if(ShouldFetchValueBank()) {
            current_memory_step = MS_FETCH_VALUE_BANK;
            current_uc_set_pc--;
        } else {
            // we're done with memory, but for R-W-M instructions (instructions where we fetch and write the same
            // memory), we have to simulate an extra
            if(is_memory_store) {
                current_memory_step = MS_MODIFY_WAIT;
            } else {
                current_memory_step = MS_MODIFY;
            }
        }
        break;

    case MS_FETCH_VALUE_BANK:
        assert(false); //TODO
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

    case MS_ADD_DL_REGISTER:
        // TODO Zero page addressing "wraps" in emulation mode, whereas in Native mode it rolls into the next page.

        // the high byte of the direct page register is already added, so add the low byte with carry
        operand_address.as_word += (u16)registers.dl;

        // go onto the next step in direct page
        SetMemoryStepAfterDirectPageAdded(is_memory_fetch);
        break;

    case MS_ADD_X_REGISTER:
    case MS_ADD_Y_REGISTER:
        // TODO only add 16-bit X and Y when X bit is 0
        // TODO in emulation mode, this wraps the low byte in direct page but not in absolute
        // TODO in native mode it never wraps
        // TODO use IsDirectAddressingMode()/IsAbsoluteAddressingMode() ?
        switch(current_addressing_mode) {
        case AM_DIRECT_INDEXED_X:
        case AM_DIRECT_INDEXED_Y:
        case AM_DIRECT_INDEXED_X_INDIRECT:
            operand_address.as_byte += (current_memory_step == MS_ADD_X_REGISTER) ? registers.xl : registers.yl;
            break;

        case AM_ABSOLUTE_INDEXED_X:
        case AM_ABSOLUTE_INDEXED_Y:
        case AM_ABSOLUTE_INDEXED_X_INDIRECT:
        case AM_DIRECT_INDIRECT_INDEXED_Y:  // for this mode, the indirect address is a word and doesn't page wrap
            operand_address.as_word += (u16)((current_memory_step == MS_ADD_X_REGISTER) ? registers.xl : registers.yl);
            break;
        }

        // go onto the next step in the direct page
        SetMemoryStepAfterIndexRegisterAdded(is_memory_fetch);
        break;
    }
}

bool CPU65C816::ShouldFetchOperandHigh()
{
    // TODO just use lookup tables once all the addressing modes are implemented
    switch(current_addressing_mode) {
    case AM_IMMEDIATE:
    case AM_DIRECT_PAGE:
    case AM_DIRECT_INDEXED_X:
    case AM_DIRECT_INDEXED_Y:
    case AM_DIRECT_INDIRECT:
    case AM_DIRECT_INDEXED_X_INDIRECT:
    case AM_DIRECT_INDIRECT_INDEXED_Y:
        return false;

    case AM_IMMEDIATE_WORD:
    case AM_ABSOLUTE:
    case AM_ABSOLUTE_INDEXED_X:
    case AM_ABSOLUTE_INDEXED_Y:
    case AM_ABSOLUTE_INDEXED_X_INDIRECT:
    case AM_ABSOLUTE_INDIRECT:
        return true;

    default:
        assert(false); // unimplemented
        return false;
    }
}

bool CPU65C816::ShouldFetchOperandBank()
{
    // TODO just use lookup tables once all the addressing modes are implemented
    switch(current_addressing_mode) {
    case AM_IMMEDIATE_WORD:
    case AM_ABSOLUTE:
    case AM_ABSOLUTE_INDEXED_X:
    case AM_ABSOLUTE_INDEXED_Y:
    case AM_ABSOLUTE_INDEXED_X_INDIRECT:
    case AM_ABSOLUTE_INDIRECT:
        return false;
        break;

    default:
        assert(false); // unimplemented
        return false;
    }
}

bool CPU65C816::ShouldFetchValueHigh()
{
    // TODO reading 16-bit and 24-bit values
    // TODO move onto next byte, if required
    // TODO how to tell if fetch is for memory or index?
    //!if(IsWordMemoryEnabled()) {
    //!}

    switch(current_addressing_mode) {
    case AM_ABSOLUTE_INDIRECT:
    case AM_ABSOLUTE_INDEXED_X_INDIRECT:
        // these indirect values are usedused with JMP and JSR, and they require a word /value/
        return true;

    default:
        return false;
    }
}

bool CPU65C816::ShouldFetchIndirectBank()
{
    // nothing using this yet
    return false;
}

bool CPU65C816::ShouldFetchValueBank()
{
    // nothing using this yet
    return false;
}


void CPU65C816::SetMemoryStepAfterOperandFetch(bool is_memory_fetch)
{
    switch(current_addressing_mode) {
    case AM_IMMEDIATE: // always at least a low byte but may or may not contain a high byte depending on M/X
        // move the operand to data since it's used as data
        intermediate_data.as_byte = operand_address.as_byte;
        intermediate_data_size = 1; // default to byte size, might be increased later

        // for immediate bytes, we're done
        current_memory_step = MS_MODIFY;
        break;

    case AM_IMMEDIATE_WORD:
        // move the operand to data since it's used as data
        intermediate_data.as_word = operand_address.as_word;
        intermediate_data_size = 2;

        // we've read the operand and it's time to do something with it
        current_memory_step = MS_MODIFY;
        break;

    case AM_DIRECT_PAGE:
    case AM_DIRECT_INDEXED_X:
    case AM_DIRECT_INDEXED_Y:
    case AM_DIRECT_INDIRECT:
    case AM_DIRECT_INDEXED_X_INDIRECT:
    case AM_DIRECT_INDIRECT_INDEXED_Y:
        // direct page is always in bank 0
        operand_address.bank_byte = 0;

        // as we latch the direct page operand, we put it in the low byte and the high byte gets
        // the high byte of the direct page register. in the hardware, this happens with a nice OR.
        operand_address.high_byte = registers.dh;

        // however, if the low byte of the direct page register is non-zero, we need to add it which requires another cycle
        if(registers.dl) {
            current_memory_step = MS_ADD_DL_REGISTER;
            current_uc_set_pc--;
        } else {
            SetMemoryStepAfterDirectPageAdded(is_memory_fetch);
        }
        break;

    case AM_ABSOLUTE:
    case AM_ABSOLUTE_INDEXED_X:
    case AM_ABSOLUTE_INDEXED_Y:
    case AM_ABSOLUTE_INDEXED_X_INDIRECT:
        // absolute uses data bank
        operand_address.bank_byte = registers.dbr;

        // TODO move onto the next step, which will depend on the addressing mode
        //! SetMemoryStepAfterAbsoluteOperand();
        // for now, we will just read memory
        switch(current_addressing_mode) {
        case AM_ABSOLUTE:
            if(is_memory_fetch) {
                current_memory_step = MS_FETCH_VALUE_LOW;
                current_uc_set_pc--;
            } else {
                current_memory_step = MS_MODIFY;
            }
            break;

        case AM_ABSOLUTE_INDEXED_X:
        case AM_ABSOLUTE_INDEXED_X_INDIRECT:
            current_memory_step = MS_ADD_X_REGISTER;
            current_uc_set_pc--;
            break;

        case AM_ABSOLUTE_INDEXED_Y:
            current_memory_step = MS_ADD_Y_REGISTER;
            current_uc_set_pc--;
            break;
        }
        break;

    case AM_ABSOLUTE_INDIRECT:
        // this addressing mode is only available during a fetch
        assert(is_memory_fetch);

        // AM_ABSOLUTE_INDIRECT is only used with JMP (a) and JML (a), and UC_STORE_PC wants the the indirect
        // address, not the value at the address, so we use FETCH_VALUE not FETCH_INDIRECT
        //
        // absolute read uses data bank
        operand_address.bank_byte = registers.dbr;

        // always move on to read the value
        current_memory_step = MS_FETCH_VALUE_LOW;
        current_uc_set_pc--;
        break;

    default:
        // all other cases are done, process the data and/or store it as necessary
        current_memory_step = MS_MODIFY;
        break;
    }
}

// After the indirect address has been fetched, we may need to do more work
// otherwise, fetch the value in memory or go on to process it
void CPU65C816::SetMemoryStepAfterIndirectAddressFetch(bool is_memory_fetch)
{
    switch(current_addressing_mode) {
    case AM_DIRECT_INDIRECT:
    case AM_DIRECT_INDEXED_X_INDIRECT:
        // the last step in this addressing mode was to fetch the indirect address,
        // so move on to fetch the value or execute the opcode
        if(is_memory_fetch) {
            current_memory_step = MS_FETCH_VALUE_LOW;
            current_uc_set_pc--;
        } else {
            current_memory_step = MS_MODIFY;
        }
        break;

    case AM_DIRECT_INDIRECT_INDEXED_Y:
        // on post-indexed Y, we now need to add Y to the address
        current_memory_step = MS_ADD_Y_REGISTER;
        current_uc_set_pc--;
        break;

    default:
        assert(false); // invalid addressing mode fetching something indirect
        break;
    }
}

// After a direct page address has been fully set up in operand_address, determine the next memory step
void CPU65C816::SetMemoryStepAfterDirectPageAdded(bool is_memory_fetch)
{
    // determine the next step in the operation
    switch(current_addressing_mode) {
    case AM_DIRECT_PAGE:
        // if all we wanted was the direct page address, then
        // operand_address now contains the direct page address. we can go read the value from memory
        // or if we're only computing the address for a store operation, then move on to execute the opcode
        if(is_memory_fetch) {
            current_memory_step = MS_FETCH_VALUE_LOW;
            current_uc_set_pc--;     // stay on the same uC instruction
        } else {
            current_memory_step = MS_MODIFY;
        }
        break;

    case AM_DIRECT_INDEXED_X:
    case AM_DIRECT_INDEXED_X_INDIRECT:
        // for direct-indexed-x and direct-indexed-x-indirect, add the X register
        current_memory_step = MS_ADD_X_REGISTER;
        current_uc_set_pc--;
        break;

    case AM_DIRECT_INDEXED_Y:
        // for direct-indexed-y, add the Y register
        current_memory_step = MS_ADD_Y_REGISTER;
        current_uc_set_pc--;
        break;

    case AM_DIRECT_INDIRECT:
    case AM_DIRECT_INDIRECT_INDEXED_Y:
        // for direct-indirect-***, we first have to fetch the indirect address before doing anything else, like adding Y
        current_memory_step = MS_FETCH_INDIRECT_LOW;
        current_uc_set_pc--;
        break;
    }
}

// After an index register has been added to the address in operand_address, determine the next memory step
void CPU65C816::SetMemoryStepAfterIndexRegisterAdded(bool is_memory_fetch)
{
    switch(current_addressing_mode) {
    case AM_DIRECT_INDEXED_X:
    case AM_DIRECT_INDEXED_Y:
    case AM_DIRECT_INDIRECT_INDEXED_Y:
    case AM_ABSOLUTE_INDEXED_X:
    case AM_ABSOLUTE_INDEXED_Y:
        // if adding a register offset is the last operation to do, then we're done now and
        // operand_address now contains the direct/absolute/indirect+x/y address. we can go read the value from memory
        // or if we're only computing the address for a store operation later, move on to execute the opcode
        if(is_memory_fetch) {
            current_memory_step = MS_FETCH_VALUE_LOW;
            current_uc_set_pc--;     // stay on the same uC instruction
        } else {
            current_memory_step = MS_MODIFY;
        }
        break;

    case AM_DIRECT_INDEXED_X_INDIRECT:
        // in the indirect modes, fetch the indirect address
        current_memory_step = MS_FETCH_INDIRECT_LOW;
        current_uc_set_pc--;
        break;

    case AM_ABSOLUTE_INDEXED_X_INDIRECT:
        // absolute indexed x indirect is only used with JMP and JSR, and they want the actual
        // indirect address, not the value of the data pointed to by the indirect address. so we'll use
        // a FETCH_VALUE here to load intermediate_data with the address so that UC_STORE_PC gets the right value
        current_memory_step = MS_FETCH_VALUE_LOW;
        current_uc_set_pc--;
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

            bool is_memory_fetch = ((current_uc_opcode & UC_FETCH_MASK) == UC_FETCH_MEMORY);

            switch(current_addressing_mode) {
                case AM_VECTOR:
                    current_memory_step = MS_FETCH_VECTOR_LOW;
                    break;

                case AM_IMMEDIATE:
                case AM_IMMEDIATE_WORD:
                case AM_DIRECT_PAGE:
                case AM_DIRECT_INDEXED_X:
                case AM_DIRECT_INDEXED_Y:
                case AM_DIRECT_INDIRECT:
                case AM_DIRECT_INDEXED_X_INDIRECT:
                case AM_DIRECT_INDIRECT_INDEXED_Y:
                case AM_ABSOLUTE:
                case AM_ABSOLUTE_INDEXED_X:
                case AM_ABSOLUTE_INDEXED_Y:
                case AM_ABSOLUTE_INDEXED_X_INDIRECT:
                case AM_ABSOLUTE_INDIRECT:
                    current_memory_step = MS_FETCH_OPERAND_LOW;
                    break;

                case AM_STACK:
                    if(is_memory_fetch) {
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

        // when we have UC_STORE_MEMORY and not UC_FETCH_MEMORY, we can fetch register contents in the same
        // cycle that the opcode was decoded, reducing the cycle counts by 1.  data from memory requires an extra
        // cycle to process since it has to be latched before the ALU can take it
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

            // if no memory fetch is happening, immediately perform the operation
            if(current_memory_step == MS_INIT) current_memory_step = MS_MODIFY;
            break;

        case UC_FETCH_X:
            cout << "[cpu65c816] fetching X" << endl;
            intermediate_data.as_byte = registers.xl;
            intermediate_data_size = 1;

            // if no memory fetch is happening, immediately perform the operation
            if(current_memory_step == MS_INIT) current_memory_step = MS_MODIFY;
            break;

        case UC_FETCH_D:
            cout << "[cpu65c816] fetching D" << endl;
            intermediate_data.as_word = registers.d;
            intermediate_data_size = 2;

            // if no memory fetch is happening, immediately perform the operation
            if(current_memory_step == MS_INIT) current_memory_step = MS_MODIFY;
            break;

        case UC_FETCH_S:
            cout << "[cpu65c816] fetching S" << endl;
            intermediate_data.as_word = registers.s;
            intermediate_data_size = 2;

            // if no memory fetch is happening, immediately perform the operation
            if(current_memory_step == MS_INIT) current_memory_step = MS_MODIFY;
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
        case MS_FETCH_VECTOR_LOW:
        case MS_FETCH_VECTOR_HIGH:
            cout << "vector address byte " << (current_memory_step - MS_FETCH_VECTOR_LOW) << endl;
            pins.vpa.AssertHigh(); // assert VPA
            pins.vp_n.AssertLow(); // for vector fetch only, assert VPn low
            data_rw_bank    = operand_address.bank_byte;  // vector is always in bank 0, but use it anyway
            data_rw_address = operand_address.as_word + (current_memory_step - MS_FETCH_VECTOR_LOW); // use the vector address
            break;

        case MS_FETCH_OPERAND_LOW:
        case MS_FETCH_OPERAND_HIGH:
        case MS_FETCH_OPERAND_BANK:
            cout << "instruction operand byte " << (current_memory_step - MS_FETCH_OPERAND_LOW) << endl;
            pins.vpa.AssertHigh(); // assert VPA
            data_rw_bank    = registers.pbr; // operands use the program bank
            data_rw_address = registers.pc;  // PC is incremented in FinishInstructionCycle() on operand fetches
            break;

        case MS_FETCH_INDIRECT_LOW:
        case MS_FETCH_INDIRECT_HIGH:
        case MS_FETCH_INDIRECT_BANK:
            cout << "indirect address byte " << (current_memory_step - MS_FETCH_INDIRECT_LOW) << endl;
            pins.vda.AssertHigh(); // assert VDA
            data_rw_bank    = operand_address.bank_byte; // indirect address comes from data bank
            data_rw_address = operand_address.as_word;   // address incremented in FinishInstructionCycle() on operand fetches
            break;
       
        case MS_FETCH_VALUE_LOW:
        case MS_FETCH_VALUE_HIGH:
        case MS_FETCH_VALUE_BANK:
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

        case MS_ADD_DL_REGISTER:
            cout << "adding D register to intermediate address (no fetch)" << endl;
            break;

        case MS_ADD_X_REGISTER:
            cout << "adding X register to intermediate address (no fetch)" << endl;
            break;

        case MS_ADD_Y_REGISTER:
            cout << "adding Y register to intermediate address (no fetch)" << endl;
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

