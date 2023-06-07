// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#include <cassert>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>

#include "magic_enum.hpp"

#include "systems/nes/cartridge.h"
#include "systems/nes/defines.h"
#include "systems/nes/disasm.h"
#include "systems/nes/expressions.h"
#include "systems/nes/label.h"
#include "systems/nes/memory.h"
#include "systems/nes/system.h"

#include "util.h"

using namespace std;

namespace Systems::NES {

System::System()
    : ::BaseSystem(), disassembling(false)
{
    disassembler = make_shared<Disassembler>();

    define_created = make_shared<define_created_t>();
    label_created = make_shared<label_created_t>();
    label_deleted = make_shared<label_deleted_t>();
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
    
    cpu_ram       = make_shared<RAMRegion>(selfptr, "RAM", 0x0000, 0x0800); // 0x0000-0x2000 mirrored every 0x0800 bytes
    cpu_ram->InitializeEmpty();

    ppu_registers = make_shared<PPURegistersRegion>(selfptr); // 0x2000-0x3FFF
    ppu_registers->InitializeEmpty();

    io_registers  = make_shared<IORegistersRegion>(selfptr);  // 0x4000-0x401F
    io_registers->InitializeEmpty();

    cartridge     = make_shared<Cartridge>(selfptr);          // 0x6000-0xFFFF
}

void System::CreateDefaultDefines()
{
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

    p.address = 0x4000; CreateLabel(p, "SQ1_VOL");
    p.address = 0x4001; CreateLabel(p, "SQ1_SWEEP");
    p.address = 0x4002; CreateLabel(p, "SQ1_LO");
    p.address = 0x4003; CreateLabel(p, "SQ1_HI");
    p.address = 0x4004; CreateLabel(p, "SQ2_VOL");
    p.address = 0x4005; CreateLabel(p, "SQ2_SWEEP");
    p.address = 0x4006; CreateLabel(p, "SQ2_LO");
    p.address = 0x4007; CreateLabel(p, "SQ2_HI");
    p.address = 0x4008; CreateLabel(p, "TRI_LINEAR");
    p.address = 0x400A; CreateLabel(p, "TRI_LO");
    p.address = 0x400B; CreateLabel(p, "TRI_HI");
    p.address = 0x400C; CreateLabel(p, "NOISE_VOL");
    p.address = 0x400E; CreateLabel(p, "NOISE_HI");
    p.address = 0x400F; CreateLabel(p, "NOISE_LO");
    p.address = 0x4010; CreateLabel(p, "DMC_FREQ");
    p.address = 0x4011; CreateLabel(p, "DMC_RAW");
    p.address = 0x4012; CreateLabel(p, "DMC_START");
    p.address = 0x4013; CreateLabel(p, "DMC_LEN");
    p.address = 0x4014; CreateLabel(p, "OAMDMA");
    p.address = 0x4015; CreateLabel(p, "SND_CHN");
    p.address = 0x4016; CreateLabel(p, "JOY1");
    p.address = 0x4017; CreateLabel(p, "JOY2");

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
    } else if(where.address < 0x6000) {
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

int System::GetNumMemoryRegions() const
{
    return 3 + cartridge->GetNumMemoryRegions();
}

std::shared_ptr<MemoryRegion> System::GetMemoryRegionByIndex(int i)
{
    switch(i) {
    case 0:
        return cpu_ram;
    case 1:
        return ppu_registers;
    case 2:
        return io_registers;
    default:
        return cartridge->GetMemoryRegionByIndex(i - 3);
    }
    return nullptr;
}

std::shared_ptr<MemoryRegion> System::GetMemoryRegion(GlobalMemoryLocation const& where)
{
    assert(!where.is_chr); // TODO

    if(where.address < cpu_ram->GetEndAddress()) {
        return cpu_ram;
    } else if(where.address < 0x2000) { // empty space
        return nullptr;
    } else if(where.address < ppu_registers->GetEndAddress()) {
        return ppu_registers;
    } else if(where.address < io_registers->GetEndAddress()) {
        return io_registers;
    } else if(where.address < 0x6000) { // empty space
        return nullptr;
    } else {
        return cartridge->GetMemoryRegion(where);
    }

    return nullptr;
}

std::shared_ptr<MemoryObject> System::GetMemoryObject(GlobalMemoryLocation const& where, int* offset)
{
    if(auto memory_region = GetMemoryRegion(where)) {
        return memory_region->GetMemoryObject(where, offset);
    }

    return nullptr;
}

void System::MarkMemoryAsUndefined(GlobalMemoryLocation const& where, u32 byte_count)
{
    auto memory_region = GetMemoryRegion(where);
    memory_region->MarkMemoryAsUndefined(where, byte_count);
}

void System::MarkMemoryAsWords(GlobalMemoryLocation const& where, u32 byte_count)
{
    auto memory_region = GetMemoryRegion(where);
    memory_region->MarkMemoryAsWords(where, byte_count);
}

void System::MarkMemoryAsString(GlobalMemoryLocation const& where, u32 byte_count)
{
    auto memory_region = GetMemoryRegion(where);
    memory_region->MarkMemoryAsString(where, byte_count);
}

shared_ptr<ExpressionNodeCreator> System::GetNodeCreator()
{
    return make_shared<ExpressionNodeCreator>();
}

// convert names into labels or defines
// at the root, convert Immediate, Accum, and IndexedX/Y
bool System::ExploreExpressionNodeCallback(shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const& parent, int depth, void* userdata)
{
    ExploreExpressionNodeData* explore_data = (ExploreExpressionNodeData*)userdata;

    // check names, and convert them into appropriate expression nodes
    if(auto name = dynamic_pointer_cast<BaseExpressionNodes::Name>(node)) {
        auto str = name->GetString();
        auto strl = strlower(str);

        // convert to Accum mode only at depth 0
        if(depth == 0) {
            if(strl == "a") { // convert accumulator
                if(explore_data->allow_modes) {
                    node = GetNodeCreator()->CreateAccum(str);
                    cout << "Made Accum node" << endl;
                } else {
                    explore_data->errmsg = "Register name not allowed here";
                    return false;
                }
            }
        }

        // check names A, X, Y
        if(strl == "x" || strl == "y" || strl == "a") {
            // we may see them as indexed values at Layer 1, but only if the parent is an expression list
            // we check length of list and position within the list later
            auto parent_list = dynamic_pointer_cast<BaseExpressionNodes::ExpressionList>(parent);
            if(!explore_data->allow_modes || !parent_list || depth > 1) {
                stringstream ss;
                ss << "Invalid use of register name '" << str << "'";
                explore_data->errmsg = ss.str();
                return false;
            }
        }

        // try to look up the label
        bool was_a_thing = false;
        if(explore_data->allow_labels) {
            if(auto label = FindLabel(str)) {
                // label exists, create a default display for it
                stringstream ss;
                ss << hex << setfill('0') << uppercase << "$";
                auto loc = label->GetMemoryLocation();
                if(loc.address < 0x100) ss << setw(2);
                else ss << setw(4);
                ss << loc.address;

                // replace the current node with a Label expression node
                node = GetNodeCreator()->CreateLabel(loc, label->GetIndex(), ss.str());

                // make sure offset is updated
                auto label_node = dynamic_pointer_cast<ExpressionNodes::Label>(node);
                label_node->Update();
                
                // enable long mode if wanted
                if(explore_data->long_mode_labels) label_node->SetLongMode(true);

                explore_data->labels.push_back(label);
                was_a_thing = true;
            }
        }

        // look up define and create Define expression node
        if(!was_a_thing && explore_data->allow_defines) {
            if(auto define = FindDefine(str)) {
                // define exists, replace the current node with a Define expression node
                node = GetNodeCreator()->CreateDefine(define);

                explore_data->defines.push_back(define);
                was_a_thing = true;
            }
        }

        if(!was_a_thing) {
            explore_data->undefined_names.push_back(str);
        }
    }

    // only allow Immediate at the root node
    if(auto immediate = dynamic_pointer_cast<ExpressionNodes::Immediate>(node)) {
        if(depth != 0) {
            explore_data->errmsg = "Invalid use of Immediate (#) mode";
            return false;
        }
    }

    // Convert indexed addressing modes at the root. Expressions nested one layer deep have already
    // been created in the Expression::ParseParenExpression function
    if(auto list = dynamic_pointer_cast<BaseExpressionNodes::ExpressionList>(node)) {
        if(!explore_data->allow_modes) {
            explore_data->errmsg = "Invalid use of indexing mode";
            return false;
        }

        if(list->GetSize() != 2) {
            explore_data->errmsg = "Invalid expression list (can only be length 2)";
            return false;
        }

        string display;
        auto name = dynamic_pointer_cast<BaseExpressionNodes::Name>(list->GetNode(1, &display));
        string str = name ? strlower(name->GetString()) : "";
        if(str != "x" && str != "y") {
            explore_data->errmsg = "Invalid index (must be X or Y)";
            return false;
        }

        // convert the node into IndexedX or Y
        display = display + name->GetString();
        auto value = list->GetNode(0);
		auto node_creator = dynamic_pointer_cast<ExpressionNodeCreator>(Expression().GetNodeCreator());
        if(str == "x") {
            node = node_creator->CreateIndexedX(value, display);
        } else {
            node = node_creator->CreateIndexedY(value, display);
        }
    }

    if(auto deref = dynamic_pointer_cast<BaseExpressionNodes::DereferenceOp>(node)) {
        if(!explore_data->allow_deref) {
            explore_data->errmsg = "Dereference not valid in this context";
            return false;
        }
    }

    return true;
}

bool System::SetOperandExpression(GlobalMemoryLocation const& where, shared_ptr<Expression>& expr, std::string& errmsg)
{
    auto memory_region = GetMemoryRegion(where);
    auto memory_object = GetMemoryObject(where);
    if(!memory_region || !memory_object) {
        errmsg = "Invalid address"; // shouldn't happen
        return false; 
    }

    if(memory_object->type == MemoryObject::TYPE_UNDEFINED) {
        errmsg = "Cannot set operand expression for undefined data types";
        return false;
    }

    // Loop over every node (and change them to system nodes if necessary), validating some things along the way
    if(!FixupExpression(expr, errmsg, true, true, false, true)) return false;

    // Now we need to do one more thing: determine the addressing mode of the expression and match it to
    // the addressing mode of the current opcode. the size of the operand is encoded into the addressing mode
    // so we also need to make sure the expression evaluates to something that fits in that size
    ADDRESSING_MODE expression_mode;
	s64 operand_value;
    if(!DetermineAddressingMode(expr, &expression_mode, &operand_value, errmsg)) return false;

    // Now we check that the resulting mode matches the addressing mode of the opcode
    // TODO in the future, we will allow changing of the opcode to match the new addressing mode, but that requires a little 
    // bit more in the disassembler
    switch(memory_object->type) {
    case MemoryObject::TYPE_CODE:
    {
        ADDRESSING_MODE opmode = disassembler->GetAddressingMode(memory_object->code.opcode);

        // Some special case exceptions:
        // 1. when the opcode is absolute but fits zero page
        // 2. when the opcode is abs,x or abs,y but fits zp,x or zp,y
        // 3. when the opcode is relative
        if(opmode == AM_ABSOLUTE   && expression_mode == AM_ZEROPAGE)   expression_mode = opmode; // upgrade
        if(opmode == AM_ABSOLUTE_X && expression_mode == AM_ZEROPAGE_X) expression_mode = opmode;
        if(opmode == AM_ABSOLUTE_Y && expression_mode == AM_ZEROPAGE_Y) expression_mode = opmode;
        if(opmode == AM_RELATIVE   && expression_mode == AM_ABSOLUTE)   expression_mode = opmode;

        if(opmode != expression_mode) { // if the mod still doesn't match
            stringstream ss;
            ss << "Expression addressing mode (" << magic_enum::enum_name(expression_mode) 
               << ") does not match opcode addressing mode (" << magic_enum::enum_name(opmode) << ")";
            errmsg = ss.str();
            return false;
        }

        // Convert relative operand_value 
        if(expression_mode == AM_RELATIVE) {
            operand_value = operand_value - (where.address + 2);
            operand_value = (u64)operand_value & 0xFF;
        }

        // Now we need to validate that operand_value matches the actual data
        u16 operand = (u16)memory_object->code.operands[0];
        if(memory_object->GetSize() == 3) {
            operand |= ((u16)memory_object->code.operands[1] << 8);
            operand_value = (u64)operand_value & 0xFFFF;
        }

        if(operand != operand_value) {
            stringstream ss;
            ss << hex << setfill('0') << uppercase;
            ss << "Expression value ($" << setw(4) << (int)operand_value 
               << ") does not evaluate to instruction operand value ($" << setw(4) << operand << ")";
            errmsg = ss.str();
            return false;
        }

        // all these fuckin checks mean the expression is finally acceptable
        break;
    }

    case MemoryObject::TYPE_BYTE:
        // Only allow AM_ZEROPAGE
    case MemoryObject::TYPE_WORD:
        // Only allow AM_ABSOLUTE
        break;

    default:
        assert(false);
        break;
    }

    memory_region->SetOperandExpression(where, expr);
    return true;
}

// Determine the addressing mode from an expression. Returns the operand value for further checks
// Returns true if the operand value is determinable and the operand value fits the addressing mode
bool System::DetermineAddressingMode(shared_ptr<Expression>& expr, ADDRESSING_MODE* addressing_mode, s64* operand_value, string& errmsg)
{
    assert(operand_value != nullptr);

    auto root = expr->GetRoot();
    if(auto acc = dynamic_pointer_cast<ExpressionNodes::Accum>(root)) { // check if the root is an Accum
        // Accum has no child nodes, so we can easily succeed here
        *addressing_mode = AM_ACCUM;
        *operand_value   = 0;
        return true;
    } else if(auto imm = dynamic_pointer_cast<ExpressionNodes::Immediate>(root)) { // check if the root is an Immediate
        *addressing_mode = AM_IMMEDIATE;

        // For Immediate to be valid, the expression must be evaluable and be <= 255
		auto value = imm->GetValue();
        if(!value->Evaluate(operand_value, errmsg)) return false;

        if(*operand_value < 0 || *operand_value > 255) {
            stringstream ss;
            ss << "Immediate operand is out of range (0-255, got " << *operand_value << ")";
            errmsg = ss.str();
            return false;
        }

        return true;
    } else if(auto ix = dynamic_pointer_cast<ExpressionNodes::IndexedX>(root)) { // check if root is indexed x
        // We have ZP,X or ABS,X, and neither can be indirect. ZP/ABS is determined based on the evaluation of the expression
        auto base = ix->GetBase();
        if(auto parens = dynamic_pointer_cast<BaseExpressionNodes::Parens>(base)) {
            errmsg = "No Indirect-post-indexed X mode available";
            return false;
        }

        // Determine size of operand
        if(!base->Evaluate(operand_value, errmsg)) return false;

        if(*operand_value < 0 || *operand_value > 255) {
            *addressing_mode = AM_ABSOLUTE_X;
        } else {
            *addressing_mode = AM_ZEROPAGE_X;
        }

        return true;
    } else if(auto iy = dynamic_pointer_cast<ExpressionNodes::IndexedY>(root)) { // check if root is indexed y
        // We have ZP,Y or ABS,Y or (ZP),Y
        auto base = iy->GetBase();

        // post-indexed if base is convertable to parentheses
        bool post_indexed = (bool)dynamic_pointer_cast<BaseExpressionNodes::Parens>(base);

        // Determine size of operand
        if(!base->Evaluate(operand_value, errmsg)) return false;

        if(*operand_value < 0 || *operand_value > 255) {
            // There is no (ABS),Y
            if(post_indexed) {
                errmsg = "No Indirect-post-indexed Y for absolute base address available";
                return false;
            }
            *addressing_mode = AM_ABSOLUTE_Y;
        } else {
            if(post_indexed) {
                *addressing_mode = AM_INDIRECT_Y;
            } else {
                *addressing_mode = AM_ZEROPAGE_X;
            }
        }

        return true;
    } else if(auto parens = dynamic_pointer_cast<BaseExpressionNodes::Parens>(root)) { // check if root is indirect
        auto value = parens->GetValue();
        if(auto ix = dynamic_pointer_cast<ExpressionNodes::IndexedX>(root)) { // check if value is indexed x
            // We may have (ZP,X), make sure operand fits in zp
            auto base = ix->GetBase();

            if(!base->Evaluate(operand_value, errmsg)) return false;

            if(*operand_value < 0 || *operand_value > 255) {
                errmsg = "No indirect-pre-indexed X for absolute base address available";
                return false;
            }

            *addressing_mode = AM_INDIRECT_X;
        } else {
            // We have only (ABS)
            if(!value->Evaluate(operand_value, errmsg)) return false;
            *addressing_mode = AM_INDIRECT;
        }

        return true;
    } else {
        // Now we either have ZP or ABS direct and the expression has to be evaluable. 
        if(!root->Evaluate(operand_value, errmsg)) return false;

        if(*operand_value < 0 || *operand_value > 255) {
            *addressing_mode = AM_ABSOLUTE;
        } else {
            *addressing_mode = AM_ZEROPAGE;
        }

        return true;
    }
}

bool System::FixupExpression(shared_ptr<BaseExpression> const& expr, string& errmsg,
        bool allow_labels, bool allow_defines, bool allow_deref, bool allow_addressing_modes, bool long_mode_labels)
{
    ExploreExpressionNodeData explore_data {
        .errmsg           = errmsg,
        .allow_modes      = allow_addressing_modes,
        .allow_labels     = allow_labels,
        .allow_defines    = allow_defines,
        .allow_deref      = allow_deref,
        .long_mode_labels = long_mode_labels
    };

    // Loop over every node (and change them to system nodes if necessary), validating some things along the way
    auto cb = std::bind(&System::ExploreExpressionNodeCallback, this, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4);
    return expr->Explore(cb, &explore_data);
}

shared_ptr<Define> System::AddDefine(string const& name, string const& expression_string, string& errmsg)
{
    int errloc;

    // evaluate 'name' and make sure we get a name node
    auto nameexpr = make_shared<Expression>();
    if(!nameexpr->Set(name, errmsg, errloc) || !(bool)dynamic_pointer_cast<BaseExpressionNodes::Name>(nameexpr->GetRoot())) {
        errmsg = "Invalid name for Define";
        return nullptr;
    }

    string define_name = (dynamic_pointer_cast<BaseExpressionNodes::Name>(nameexpr->GetRoot()))->GetString();

    // does define exist?
    if(auto other = define_by_name.contains(define_name)) {
        errmsg = "Define name exists already";
        return nullptr;
    }

    // try parsing the expression, creating base Name nodes where necessary
    auto expr = make_shared<Expression>();
    if(!expr->Set(expression_string, errmsg, errloc)) return nullptr;

    // explore the expression and allow only defines
    ExploreExpressionNodeData explore_data {
        .errmsg           = errmsg,
        .allow_modes      = false,
        .allow_labels     = false,
        .allow_defines    = true,
        .allow_deref      = false,
        .long_mode_labels = false
    };

    auto cb = std::bind(&System::ExploreExpressionNodeCallback, this, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4);
    if(!expr->Explore(cb, &explore_data)) return nullptr;

    // and now the define must be evaluable!
    s64 result;
    if(!expr->Evaluate(&result, errmsg)) return nullptr;

    // define looks good, add to database
    cout << "adding define name(" << *nameexpr << ") = [" << *expr << "]" << " => " << result << endl;

    auto define = make_shared<Define>(define_name, expr);
    define->SetReferences();
    defines.push_back(define);
    define_by_name[define_name] = define;

    // notify the system of new defines
    define_created->emit(define);

    return define;
}

shared_ptr<Label> System::GetDefaultLabelForTarget(GlobalMemoryLocation const& where, bool was_user_created, int* target_offset, bool wide, string const& prefix)
{
    if(auto memory_object = GetMemoryObject(where, target_offset)) {
        if(memory_object->labels.size() == 0) { // create a label at that address if there isn't one yet
            stringstream ss;
            ss << prefix << hex << setfill('0') << uppercase;

            if(CanBank(where)) ss << setw(2) << (where.is_chr ? where.chr_rom_bank : where.prg_rom_bank);

            ss << (wide ? setw(4) : setw(2)) << where.address;

            return CreateLabel(where, ss.str(), was_user_created);
        } else {
            // return the first label
            return memory_object->labels[0];
        }
    }

    return nullptr;
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

        // and the specific listing address
        if(label_created_at.contains(where)) {
            label_created_at[where]->emit(label, was_user_created);
        }
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
            // remove label from the database
            label_database.erase(label->GetString());
            // change the label name and add the new reference to the db
            label->SetString(label_str);
            label_database[label_str] = label;
            return label;
        }
    }
            
    //cout << "[System::EditLabel] label '" << nth << "' doesn't exists" << where << endl;
    return nullptr;
}

void System::DeleteLabel(std::shared_ptr<Label> const& label)
{
    auto where = label->GetMemoryLocation();
    if(auto memory_region = GetMemoryRegion(where)) {
        if(int nth = memory_region->DeleteLabel(label); nth >= 0) {
            auto name = label->GetString();
            label_database.erase(name);

            label_deleted->emit(label, nth);

            if(label_deleted_at.contains(where)) {
                label_deleted_at[where]->emit(label, nth);
            }
        }
    }
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
            CreateDefaultOperandExpression(current_loc, true);

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

            case 0x10: // the relative branch instructions fork: don't branch + take branch
            case 0x30:
            case 0x50:
            case 0x70:
            case 0x90:
            case 0xB0:
            case 0xD0:
            case 0xF0:
            {
                u16 target = (u16)((s16)(current_loc.address + 2) + (s16)(s8)memory_object->code.operands[0]);

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

void System::CreateDefaultOperandExpression(GlobalMemoryLocation const& where, bool with_labels)
{
    auto code_region = GetMemoryRegion(where);
    auto code_object = GetMemoryObject(where);

    // only create operand expressions for code
    if(code_object->type != MemoryObject::TYPE_CODE) return;

    switch(auto am = disassembler->GetAddressingMode(code_object->code.opcode)) {
    case AM_ABSOLUTE:
    case AM_ABSOLUTE_X:
    case AM_ABSOLUTE_Y:
    case AM_ZEROPAGE:
    case AM_ZEROPAGE_X:
    case AM_ZEROPAGE_Y:
    case AM_INDIRECT:
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

        // only for valid destination addresses do we create a label
        // can't call GetDefaultLabelForTarget if is_valid is false because target_location might still be
        // set up to point to something in the wrong bank
        int target_offset = 0;
        bool wide = !(target < 0x100);
        string prefix = isrel ? "." : (wide ? "L_" : "zp_");
        shared_ptr<Label> label = (is_valid && with_labels) ? GetDefaultLabelForTarget(target_location, false, &target_offset, wide, prefix) : nullptr;

        // now create an expression for the operands
        auto expr = make_shared<Expression>();
        auto nc = dynamic_pointer_cast<ExpressionNodeCreator>(expr->GetNodeCreator());

        // format the operand label display string
        char buf[6];
        if(is16 || isrel) {
            snprintf(buf, sizeof(buf), "$%04X", target_location.address);
        } else {
            snprintf(buf, sizeof(buf), "$%02X", target_location.address);
        }

        // use a label node if we know the label exist
        auto root = label ? nc->CreateLabel(target_location, 0, buf)
                          : nc->CreateConstant(target_location.address, buf);

        // wrap the address/label with whatever is necessary to format this instruction
        if(am == AM_ABSOLUTE_X || am == AM_ZEROPAGE_X) {
            root = nc->CreateIndexedX(root, ",X");
        } else if(am == AM_ABSOLUTE_Y || am == AM_ZEROPAGE_Y) {
            root = nc->CreateIndexedY(root, ",Y");
        } else if(am == AM_INDIRECT) {
            // (v)
            root = nc->CreateParens("(", root, ")");
        } else if(am == AM_INDIRECT_X) {
            // (v,X)
            root = nc->CreateIndexedX(root, ",X");
            root = nc->CreateParens("(", root, ")");
        } else if(am == AM_INDIRECT_Y) {
            // (v),Y
            root = nc->CreateParens("(", root, ")");
            root = nc->CreateIndexedY(root, ",Y");
        }

        // set the expression root
        expr->Set(root);

        // call SetOperandExpression directly on the region bypassing the System::SetOperandExpression checks, which configure
        // the addressing modes and labels, etc., which we've already done
        code_region->SetOperandExpression(where, expr);
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

        auto root = nc->CreateConstant(imm, string(buf));
        root = nc->CreateImmediate("#", root);
        expr->Set(root);

        // call SetOperandExpression directly on the region bypassing the System::SetOperandExpression checks, which configure
        // the addressing modes and labels, etc., which we've already done
        code_region->SetOperandExpression(where, expr);

        break;
    }

    case AM_ACCUM: // "A", or not, we don't care!
    {
        auto expr = make_shared<Expression>();
        auto nc = dynamic_pointer_cast<ExpressionNodeCreator>(expr->GetNodeCreator());

        auto root = nc->CreateAccum("A"); // if you don't want to type the A, leave this string blank
        expr->Set(root);

        // call SetOperandExpression directly on the region bypassing the System::SetOperandExpression checks, which configure
        // the addressing modes and labels, etc., which we've already done
        code_region->SetOperandExpression(where, expr);
        break;
    }

    case AM_IMPLIED: // implied opcodes don't have an expression, so we make it empty
    {
        auto expr = make_shared<Expression>();
        auto nc = dynamic_pointer_cast<ExpressionNodeCreator>(expr->GetNodeCreator());

        // call SetOperandExpression directly on the region bypassing the System::SetOperandExpression checks, which configure
        // the addressing modes and labels, etc., which we've already done
        code_region->SetOperandExpression(where, expr);
        break;
    }

    default:
        break;
    }
}

bool System::Save(std::ostream& os, std::string& errmsg)
{
    // save the defines
    WriteVarInt(os, defines.size());
    if(!os.good()) {
        errmsg = "Error saving defines";
        return false;
    }
    for(auto& define : defines) if(!define->Save(os, errmsg)) return false;

    // save the labels globally, as parsing expressions in memory objects that use labels
    // will need to be able to look them up at load.
    WriteVarInt(os, label_database.size());
    if(!os.good()) {
        errmsg = "Error saving labels";
        return false;
    }
    for(auto& label : label_database) if(!label.second->Save(os, errmsg)) return false;

    // save the non-cartridge memory regions
    if(!cpu_ram->Save(os, errmsg)) return false;
    if(!ppu_registers->Save(os, errmsg)) return false;
    if(!io_registers->Save(os, errmsg)) return false;

    // save the cartridge (which will save some memory regions)
    if(!cartridge->Save(os, errmsg)) return false;

    return true;
}

bool System::Load(std::istream& is, std::string& errmsg)
{
    shared_ptr<BaseSystem> base_system = shared_from_this();
    auto selfptr = dynamic_pointer_cast<System>(base_system);
    assert(selfptr);

    // load defines
    int num_defines = ReadVarInt<int>(is);
    if(!is.good()) {
        errmsg = "Error loading defines";
        return false;
    }

    for(int i = 0; i < num_defines; i++) {
        auto define = Define::Load(is, errmsg);
        if(!define) return false;

        define->SetReferences();
        defines.push_back(define);
        define_by_name[define->GetString()] = define;
    }

    cout << "[System::Load] loaded " << num_defines << " defines." << endl;

    // load labels
    int num_labels = ReadVarInt<int>(is);
    if(!is.good()) {
        errmsg = "Error loading labels";
        return false;
    }

    for(int i = 0; i < num_labels; i++) {
        auto label = Label::Load(is, errmsg);
        if(!label) return false;

        label_database[label->GetString()] = label;
    }

    cout << "[System::Load] loaded " << num_labels << " labels." << endl;

    // load RAM
    cpu_ram = make_shared<RAMRegion>(selfptr, "RAM", 0x0000, 0x0800);
    if(!cpu_ram->Load(is, errmsg)) return false;

    // load registers
    ppu_registers = make_shared<PPURegistersRegion>(selfptr); // 0x2000-0x3FFF
    if(!ppu_registers->Load(is, errmsg)) return false;

    io_registers  = make_shared<IORegistersRegion>(selfptr);  // 0x4000-0x401F
    if(!io_registers->Load(is, errmsg)) return false;

    // load cartridge
    // save the cartridge (which will save some memory regions)
    cartridge     = make_shared<Cartridge>(selfptr);          // 0x6000-0xFFFF
    if(!cartridge->Load(is, errmsg, selfptr)) return false;

    // note all references
    NoteReferences();

    return true;
}

void System::NoteReferences()
{
    // cpu_ram, ppu_registers, and io_registers aren't backed memory, so they can't refer to other memory
    cartridge->NoteReferences();
}

shared_ptr<MemoryView> System::CreateMemoryView(shared_ptr<MemoryView> const& ppu_view, shared_ptr<MemoryView> const& apu_io_view)
{
    return make_shared<SystemView>(shared_from_this(), ppu_view, apu_io_view);
}

SystemView::SystemView(shared_ptr<BaseSystem> const& _system, shared_ptr<MemoryView> const& _ppu_view, shared_ptr<MemoryView> const& _apu_io_view)
    : ppu_view(_ppu_view), apu_io_view(_apu_io_view)
{
    system = dynamic_pointer_cast<System>(_system);

    cartridge_view = dynamic_pointer_cast<CartridgeView>(system->cartridge->CreateMemoryView());
}

SystemView::~SystemView()
{
}

u8 SystemView::Peek(u16 address)
{
    if(address < 0x2000) {
        return RAM[address & 0x7FF];
    } else if(address < 0x4000) {
        return ppu_view->Peek(address & 0x1FFF);
    } else if(address < 0x6000) {
        return apu_io_view->Peek(address & 0x1FFF);
    } else {
        return cartridge_view->Peek(address);
    }
}

u8 SystemView::Read(u16 address)
{
    if(address < 0x2000) {
        return RAM[address & 0x7FF];
    } else if(address < 0x4000) {
        return ppu_view->Read(address & 0x1FFF);
    } else if(address < 0x6000) {
        return apu_io_view->Read(address & 0x1FFF);
    } else {
        return cartridge_view->Read(address);
    }
}

void SystemView::Write(u16 address, u8 value)
{
    if(address < 0x2000) {
        RAM[address & 0x7FF] = value;
    } else if(address < 0x4000) {
        ppu_view->Write(address & 0x1FFF, value);
    } else if(address < 0x6000) {
        apu_io_view->Write(address & 0x1FFF, value);
    } else {
        cartridge_view->Write(address, value);
    }
}

u8 SystemView::PeekPPU(u16 address)
{
    //cout << "[SystemView::ReadPPU] read from $" << hex << address << endl;
    if(address < 0x2000) {
        // read cartridge CHR-ROM/RAM
        return cartridge_view->PeekPPU(address);
    } else if(address < 0x4000) {
        // see the note below in WritePPU on why the funky math is performed on horizontal mirroring
        switch(cartridge_view->GetNametableMirroring()) {
        case MIRRORING_VERTICAL:
            address &= ~0x800;
            break;

        case MIRRORING_HORIZONTAL: 
            address = ((address & 0x800) >> 1) | (address & ~0xC00);
            break;
        }

        return VRAM[address & 0x7FF];
    } else {
        assert(false);
        return 0;
    }
}

u8 SystemView::ReadPPU(u16 address)
{
    //cout << "[SystemView::ReadPPU] read from $" << hex << address << endl;
    if(address < 0x2000) {
        // read cartridge CHR-ROM/RAM
        return cartridge_view->ReadPPU(address);
    } else if(address < 0x4000) {
        // see the note below in WritePPU on why the funky math is performed on horizontal mirroring
        switch(cartridge_view->GetNametableMirroring()) {
        case MIRRORING_VERTICAL:
            address &= ~0x800;
            break;

        case MIRRORING_HORIZONTAL: 
            address = ((address & 0x800) >> 1) | (address & ~0xC00);
            break;
        }

        return VRAM[address & 0x7FF];
    } else {
        assert(false);
        return 0;
    }
}

void SystemView::WritePPU(u16 address, u8 value)
{
    //cout << "[SystemView::WritePPU] write $" << hex << (int)value << " to $" << address << endl;
    if(address < 0x2000) {
        // write to cartridge CHR-RAM
        cartridge_view->WritePPU(address, value);
    } else if(address < 0x4000) {
        // we have space for 2KiB of nametables - two full nametables,
        // and our linear local space is 0-0x7FF bytes for that.  It's trivial
        // to map the first name table at 0x2000-0x23FF to our first
        // name table 0-0x3FF but depending on mirroring, we need to map
        // 0x2400, 0x2800, 0x2C00 to the second (or first!) nametable range.
        // With vertical mirroring, we have two nametables arranged horizontally, with
        // the two bottom tables as "mirrors" of the top two. Mapping the top two
        // 0x2000-0x27FF is trivial -- just take out bit 0x800.
        //
        // [A][B]
        // [A][B]
        //
        // but with horizontal mapping, we have two vertical nametables with the two right
        // nametables being mirrors of the left ones:
        //
        // [A][A]
        // [B][B]
        //
        // and we need to map 0x2000/0x2800 and 0x2400/0x2C00 to the same memory, while 0x2000 and 0x2800
        // need to be converted to the local space of 0x7FF.  We can accomplish that by applying horizontal mirroring
        // (ignore bit 0x400) and then move bit 0x800 into 0x400 for our local address range
        switch(cartridge_view->GetNametableMirroring()) {
        case MIRRORING_VERTICAL:
            // take out bit 0x800 and write like normal
            address &= ~0x800;
            break;
        case MIRRORING_HORIZONTAL: 
            // and move bit 0x800 right one, overwriting whatever was in bit 0x400 and clearing old bit 0x800
            address = ((address & 0x800) >> 1) | (address & ~0xC00);
            break;
        }

        // apply mirroring throughout 0x3000..0x3FFF as well
        VRAM[address & 0x7FF] = value;
    } else {
        assert(false);
    }
}

bool SystemView::Save(ostream& os, string& errmsg) const
{
    WriteVarInt(os, 0);

    os.write((char*)RAM, sizeof(RAM));
    os.write((char*)VRAM, sizeof(VRAM));
    if(!os.good()) goto done;

    if(!ppu_view->Save(os, errmsg)) return false;
    if(!apu_io_view->Save(os, errmsg)) return false;
    if(!cartridge_view->Save(os, errmsg)) return false;

done:
    errmsg = "Error saving SystemView";
    return os.good();
}

bool SystemView::Load(istream& is, string& errmsg)
{
    auto r = ReadVarInt<int>(is);
    assert(r == 0);

    is.read((char*)RAM, sizeof(RAM));
    is.read((char*)VRAM, sizeof(VRAM));
    if(!is.good()) goto done;

    if(!ppu_view->Load(is, errmsg)) return false;
    if(!apu_io_view->Load(is, errmsg)) return false;
    if(!cartridge_view->Load(is, errmsg)) return false;

done:
    errmsg = "Error loading SystemView";
    return is.good();
}

}
