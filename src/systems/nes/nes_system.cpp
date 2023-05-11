#include <cassert>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>

#include "systems/nes/nes_cartridge.h"
#include "systems/nes/nes_disasm.h"
#include "systems/nes/nes_expressions.h"
#include "systems/nes/nes_label.h"
#include "systems/nes/nes_listing.h"
#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_system.h"

#include "util.h"

using namespace std;

namespace NES {

System::System()
    : ::BaseSystem(), disassembling(false)
{
    disassembler = make_shared<Disassembler>();

    label_created = make_shared<label_created_t>();
    disassembly_stopped = make_shared<disassembly_stopped_t>();
}

System::~System()
{
}

// Can't be called from the constructor, so call it after creating the system
void System::CreateMemoryRegions()
{
    shared_ptr<BaseSystem> base_system = shared_from_this();
    auto selfptr = dynamic_pointer_cast<System>(base_system);
    assert(selfptr);
    
    // TODO RAM
    ppu_registers = make_shared<PPURegistersRegion>(selfptr); // 0x2000-0x3FFF
    ppu_registers->InitializeEmpty();

    io_registers  = make_shared<IORegistersRegion>(selfptr);  // 0x4000-0x401F
    io_registers->InitializeEmpty();

    cartridge     = make_shared<Cartridge>(selfptr);          // 0x6000-0xFFFF
}

void System::CreateDefaultLabels()
{
    GlobalMemoryLocation p;

    // Create the CPU vector labels
    GetEntryPoint(&p);
    CreateLabel(p, "_reset");

    p.address -= 2;
    CreateLabel(p, "_nmi");
    GetMemoryRegion(p)->MarkMemoryAsWords(p, 6); // mark the three vectors as words

    p.address += 4;
    CreateLabel(p, "_irqbrk");

    // And the labels for the registers
    p = GlobalMemoryLocation();
    p.address = 0x2000; CreateLabel(p, "PPUCONT");
    p.address = 0x2001; CreateLabel(p, "PPUMASK");
    p.address = 0x2002; CreateLabel(p, "PPUSTAT");
    p.address = 0x2003; CreateLabel(p, "OAMADDR");
    p.address = 0x2004; CreateLabel(p, "OAMDATA");
    p.address = 0x2005; CreateLabel(p, "PPUSCRL");
    p.address = 0x2006; CreateLabel(p, "PPUADDR");
    p.address = 0x2007; CreateLabel(p, "PPUDATA");

    p.address = 0x4014; CreateLabel(p, "OAMDMA");
}

// Memory
void System::GetEntryPoint(GlobalMemoryLocation* out)
{
    assert((bool)cartridge);
    zero(out);
    out->address      = 0xFFFC;
    out->prg_rom_bank = cartridge->GetResetVectorBank();
}

bool System::CanBank(GlobalMemoryLocation const& where)
{
    if(where.is_chr) {
        assert(false); // TODO
        return false;
    } else {
        // some mappers don't have switchable banks, making some disassembly look nicer
        return cartridge->CanBank(where);
    }
}

void System::GetBanksForAddress(GlobalMemoryLocation const& where, vector<u16>& out)
{
    assert(!where.is_chr);

    out.clear();

    if(where.address < 0x8000) {
        out.push_back(0);
    } else if(where.address >= 0x8000) {
        for(u16 i = 0; i < cartridge->header.num_prg_rom_banks; i++) {
            auto prg_bank = cartridge->GetProgramRomBank(i);
            if(where.address >= prg_bank->GetBaseAddress() && where.address < prg_bank->GetEndAddress()) {
                out.push_back(i);
            }
        }
    }
}

std::shared_ptr<MemoryRegion> System::GetMemoryRegion(GlobalMemoryLocation const& where)
{
    static shared_ptr<MemoryRegion> empty_ptr;
    assert(!where.is_chr); // TODO

    if(where.address < 0x2000) {
        return empty_ptr;
    } else if(where.address < ppu_registers->GetEndAddress()) {
        return ppu_registers;
    } else if(where.address < io_registers->GetEndAddress()) {
        return io_registers;
    } else if(where.address < 0x6000) { // empty space
        return empty_ptr;
    } else {
        return cartridge->GetMemoryRegion(where);
    }
}

std::shared_ptr<MemoryObject> System::GetMemoryObject(GlobalMemoryLocation const& where)
{
    if(auto memory_region = GetMemoryRegion(where)) {
        return memory_region->GetMemoryObject(where);
    }

    return nullptr;
}

void System::MarkMemoryAsUndefined(GlobalMemoryLocation const& where)
{
    auto memory_region = GetMemoryRegion(where);
    memory_region->MarkMemoryAsUndefined(where);
}

void System::MarkMemoryAsWords(GlobalMemoryLocation const& where, u32 byte_count)
{
    auto memory_region = GetMemoryRegion(where);
    memory_region->MarkMemoryAsWords(where, byte_count);
}

std::vector<std::shared_ptr<Label>> const& System::GetLabelsAt(GlobalMemoryLocation const& where)
{
    static vector<shared_ptr<Label>> empty_vector;
    if(auto memory_object = GetMemoryObject(where)) {
        return memory_object->labels;
    }
    return empty_vector;
}

shared_ptr<Label> System::GetOrCreateLabel(GlobalMemoryLocation const& where, string const& label_str, bool was_user_created)
{
    // lookup the label to see if it already exists
    auto other = label_database[label_str];
    if(other) return other;

    // create a new Label
    auto label = make_shared<Label>(where, label_str);
    label_database[label_str] = label;

    if(auto memory_region = GetMemoryRegion(where)) {
        memory_region->ApplyLabel(label);

        // notify the system of new labels
        label_created->emit(label, was_user_created);
    }

    return label;
}

shared_ptr<Label> System::CreateLabel(GlobalMemoryLocation const& where, string const& label_str, bool was_user_created)
{
    auto other = label_database[label_str];
    if(other) {
        //cout << "[System::CreateLabel] label '" << label_str << "' already exists and points to " << where << endl;
        return nullptr;
    }

    return GetOrCreateLabel(where, label_str, was_user_created);
}

shared_ptr<Label> System::EditLabel(GlobalMemoryLocation const& where, string const& label_str, int nth, bool was_user_edited)
{
    if(auto memory_object = GetMemoryObject(where)) {
        if(nth < memory_object->labels.size()) {
            auto label = memory_object->labels.at(nth);
            label->SetString(label_str);
            return label;
        }
    }
            
    //cout << "[System::EditLabel] label '" << nth << "' doesn't exists" << where << endl;
    return nullptr;
}


void System::InitDisassembly(GlobalMemoryLocation const& where)
{
    disassembly_address = where;

    disassembling = true;
}

int System::DisassemblyThread()
{
    std::deque<GlobalMemoryLocation> locations;
    locations.push_back(disassembly_address);

    while(disassembling && locations.size()) {
        GlobalMemoryLocation current_loc = locations.front();
        locations.pop_front();

        bool disassembling_inner = true;
        while(disassembling_inner) {
            auto memory_region = GetMemoryRegion(current_loc);
            auto memory_object = memory_region->GetMemoryObject(current_loc);

            // bail on this disassemble if we already know the location is code
            if(memory_object->type == MemoryObject::TYPE_CODE) {
                //cout << "[NES::System::DisassemblyThread] address " << current_loc << " is already code" << endl;
                break;
            }

            // give up if we can't even convert this data to code. the user must clear the data type first
            if(memory_object->type != MemoryObject::TYPE_UNDEFINED && memory_object->type != MemoryObject::TYPE_BYTE) {
                cout << "[NES::System::DisassemblyThread] cannot disassemble type " << memory_object->type << " at " << current_loc << endl;
                break;
            }

            u8 op = memory_object->bval;
            string inst = disassembler->GetInstruction(op);

            int size = disassembler->GetInstructionSize(op);

            // stop disassembling on unknown opcodes
            if(size == 0) {
                cout << "[NES::System::DisassemblyThread] stopping because invalid opcode $" << hex << uppercase << (int)memory_object->bval << " (" << inst << ") at "  << current_loc << endl;
                break; // break on unimplemented opcodes (TODO remove me)
            }

            // convert the memory to code
            if(!memory_region->MarkMemoryAsCode(current_loc, size)) {
                assert(false); // this shouldn't happen
                break;
            }

            // re-get the memory object JIC
            memory_object = memory_region->GetMemoryObject(current_loc);

            // create the expressions as necessary
            CreateDefaultOperandExpression(current_loc, op);

            // certain instructions must stop disassembly and others cause branches
            switch(op) {
            case 0x4C: // JMP absolute
                disassembling_inner = false;
                // fall through
            case 0x20: // JSR absolute
            {
                u16 target = (u16)memory_object->code.operands[0] | ((u16)memory_object->code.operands[1] << 8);
                GlobalMemoryLocation target_location(current_loc);
                target_location.address = target;

                if(target >= memory_region->GetBaseAddress() && target < memory_region->GetEndAddress()) { // in the same bank
                    locations.push_back(target_location);
                } else if(target >= 0x8000 && !CanBank(target_location)) {
                    locations.push_back(target_location);
                }

                break;
            }

            case 0x60: // RTS
            case 0x6C: // JMP indirect
                disassembling_inner = false;
                break;
            }

            // next PC, maybe
            current_loc = current_loc + size;
        }
    }

    // leave the dialog up for at least a moment
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    disassembling = false;
    disassembly_stopped->emit(disassembly_address);
    return 0;
}

void System::CreateDefaultOperandExpression(GlobalMemoryLocation const& where, u8 opcode)
{
    auto code_region = GetMemoryRegion(where);
    auto code_object = GetMemoryObject(where);

    switch(auto am = disassembler->GetAddressingMode(opcode)) {
    case AM_ABSOLUTE:
    case AM_ABSOLUTE_X:
    case AM_ABSOLUTE_Y:
    case AM_ZEROPAGE:
    case AM_ZEROPAGE_X:
    case AM_ZEROPAGE_Y:
    case AM_INDIRECT_X:
    case AM_INDIRECT_Y:
    case AM_RELATIVE:
    {
        // 8-bit addresses are always zero page and never ROM
        bool is16 = (am == AM_ABSOLUTE || am == AM_ABSOLUTE_X || am == AM_ABSOLUTE_Y);
        bool isrel = (am == AM_RELATIVE);

        u16 target = isrel ? (u16)((s16)(where.address + 2) + (s16)(s8)code_object->code.operands[0])
                           : (is16 ? ((u16)code_object->code.operands[0] | ((u16)code_object->code.operands[1] << 8))
                                   : (u16)code_object->code.operands[0]);

        GlobalMemoryLocation target_location;
        target_location.address = target;
        bool is_valid = true;

        // if the target is in the current bank, copy over that bank number
        // if the target is in a banked region, try to determine the bank
        // otherwise, leave it at 0 for other memory regions
        if(target >= code_region->GetBaseAddress() && target < code_region->GetEndAddress()) {
            target_location.prg_rom_bank = where.prg_rom_bank;
        } else if(CanBank(target_location)) {
            vector<u16> banks;
            GetBanksForAddress(target_location, banks);
            if(banks.size() == 1) {
                target_location.prg_rom_bank = banks[0];
            } else {
                // here we can't always ask the user for which bank, since we could be disassembling
                is_valid = false;
            }
        }

        // only for valid destination addresses do we create an expression. others get the default
        // disasembler output and the user will have to create the expression manually
        auto target_object = GetMemoryObject(target_location);
        if(target_object) {
            // create a label at that address if there isn't one yet
            stringstream ss;
            if(isrel) ss << ".";
            else      ss << "L_";
            ss << hex << setfill('0') << uppercase;
            if(CanBank(target_location)) ss << setw(2) << target_location.prg_rom_bank;
            ss << setw(4) << target_location.address;

            // no problem if this fails if the label already exists
            CreateLabel(target_location, ss.str());
        }

        // now create an expression for the operands
        auto expr = make_shared<Expression>();
        auto nc = dynamic_pointer_cast<ExpressionNodeCreator>(expr->GetNodeCreator());

        // TODO format in other bases?
        char buf[6];
        if(is16 || isrel) {
            snprintf(buf, sizeof(buf), "$%04X", target_location.address);
        } else {
            snprintf(buf, sizeof(buf), "$%02X", target_location.address);
        }

        // if the destination is not valid memory, we can't really create an OperandAddressOrLabel node
        auto root = is_valid ? nc->CreateOperandAddressOrLabel(target_object, target_location, 0, string(buf))
                             : (is16 ? nc->CreateConstantU16(target_location.address, buf)
                                     : nc->CreateConstantU8(target_location.address, buf));

        // wrap the address/label with whatever is necessary to format this instruction
        if(am == AM_ABSOLUTE_X || am == AM_ZEROPAGE_X) {
            root = nc->CreateIndexedX(root, ",X");
        } else if(am == AM_ABSOLUTE_Y || am == AM_ZEROPAGE_Y) {
            root = nc->CreateIndexedY(root, ",Y");
        } else if(am == AM_INDIRECT_X) {
            // (v,X)
            root = nc->CreateIndexedX(root, ",X");
            root = nc->CreateParens("(", root, ")");
        } else if(am == AM_INDIRECT_X) {
            // (v),Y
            root = nc->CreateParens("(", root, ")");
            root = nc->CreateIndexedY(root, ",Y");
        }

        expr->Set(root);

        // set the expression for memory object at current_selection. it'll show up immediately
        code_object->operand_expression = expr;
        break;
    }

    case AM_IMMEDIATE: // Immediate instructions don't get labels
    {
        u8 imm = code_object->code.operands[0];

        auto expr = make_shared<Expression>();
        auto nc = dynamic_pointer_cast<ExpressionNodeCreator>(expr->GetNodeCreator());

        // TODO format in other bases?
        char buf[4];
        snprintf(buf, sizeof(buf), "$%02X", imm);

        auto root = nc->CreateConstantU8(imm, string(buf));
        root = nc->CreateImmediate("#", root);
        expr->Set(root);

        // set the expression for memory object at current_selection. it'll show up immediately
        code_object->operand_expression = expr;

        break;
    }

    case AM_ACCUM: // "A", or not, we don't care!
    {
        auto expr = make_shared<Expression>();
        auto nc = dynamic_pointer_cast<ExpressionNodeCreator>(expr->GetNodeCreator());

        auto root = nc->CreateAccum("A"); // if you don't want to type the A, leave this string blank
        expr->Set(root);

        // set the expression for memory object at current_selection. it'll show up immediately
        code_object->operand_expression = expr;
        break;
    }

    case AM_IMPLIED: // implied opcodes don't have an expression, so we make it empty
    {
        auto expr = make_shared<Expression>();
        auto nc = dynamic_pointer_cast<ExpressionNodeCreator>(expr->GetNodeCreator());

        // set the expression for memory object at current_selection. it'll show up immediately
        code_object->operand_expression = expr;
        break;
    }

    default:
        break;
    }
}

}
