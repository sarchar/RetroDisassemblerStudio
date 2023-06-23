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
#include "systems/nes/enum.h"
#include "systems/nes/expressions.h"
#include "systems/nes/label.h"
#include "systems/nes/memory.h"
#include "systems/nes/system.h"

#include "windows/nes/project.h"

#include "util.h"

using namespace std;

namespace Systems::NES {

System::System()
    : ::BaseSystem(), disassembling(false)
{
    disassembler = make_shared<Disassembler>();
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

shared_ptr<MemoryRegion> System::GetMemoryRegionByIndex(int i)
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

shared_ptr<MemoryRegion> System::GetMemoryRegion(GlobalMemoryLocation const& where)
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

shared_ptr<MemoryObject> System::GetMemoryObject(GlobalMemoryLocation const& where, int* offset)
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

void System::MarkMemoryAsBytes(GlobalMemoryLocation const& where, u32 byte_count)
{
    auto memory_region = GetMemoryRegion(where);
    memory_region->MarkMemoryAsBytes(where, byte_count);
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

void System::MarkMemoryAsEnum(GlobalMemoryLocation const& where, u32 byte_count, shared_ptr<Enum> const& enum_type)
{
    auto memory_region = GetMemoryRegion(where);
    memory_region->MarkMemoryAsEnum(where, byte_count, enum_type);
}

shared_ptr<ExpressionNodeCreator> System::GetNodeCreator()
{
    return make_shared<ExpressionNodeCreator>();
}

// convert names into labels or defines
// at the root, convert Immediate, Accum, and IndexedX/Y
bool System::ExploreExpressionNodeCallback(shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const& parent, int depth, void* userdata)
{
    auto explore_data = static_cast<ExploreExpressionNodeData*>(userdata);
    explore_data->num_nodes++;

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

        // look up enum
        if(!was_a_thing && explore_data->allow_enums) {
            if(auto ee = GetEnumElement(str)) {
                // enum element exists, replace current node with EnumElement expression node
                node = GetNodeCreator()->CreateEnumElement(ee);

                explore_data->enum_elements.push_back(ee);
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

bool System::SetOperandExpression(GlobalMemoryLocation const& where, shared_ptr<Expression>& expr, string& errmsg)
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
    FixupFlags fixup_flags = FIXUP_LABELS | FIXUP_DEFINES | FIXUP_ENUMS | FIXUP_ADDRESSING_MODES;
    int num_nodes;
    if(!FixupExpression(expr, errmsg, fixup_flags, &num_nodes)) return false;

    // Now we need to do one more thing: determine the addressing mode of the expression and match it to
    // the addressing mode of the current opcode. the size of the operand is encoded into the addressing mode
    // so we also need to make sure the expression evaluates to something that fits in that size
    ADDRESSING_MODE expression_mode;
	s64 expression_value;
    if(!DetermineAddressingMode(expr, &expression_mode, &expression_value, errmsg)) return false;

    // Now we check that the resulting mode matches the addressing mode of the opcode
    // TODO in the future, we will allow changing of the opcode to match the new addressing mode, but that requires a little 
    // bit more in the disassembler
    switch(memory_object->type) {
    case MemoryObject::TYPE_CODE:
    {
        ADDRESSING_MODE opmode = disassembler->GetAddressingMode(*memory_object->data_ptr);

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

        // Convert relative expression_value 
        if(expression_mode == AM_RELATIVE) {
            expression_value = expression_value - (where.address + 2);
            expression_value = (u64)expression_value & 0xFF;
        }

        // Now we need to validate that expression_value matches the actual data
        u16 operand = (u16)memory_object->data_ptr[1];
        if(memory_object->GetSize() == 3) {
            operand |= ((u16)memory_object->data_ptr[2] << 8);
            expression_value = (u64)expression_value & 0xFFFF;
        }

        if(operand != expression_value) {
            stringstream ss;
            ss << hex << setfill('0') << uppercase;
            ss << "Expression value ($" << setw(4) << (int)expression_value 
               << ") does not evaluate to instruction operand value ($" << setw(4) << operand << ")";
            errmsg = ss.str();
            return false;
        }

        // all these fuckin checks mean the expression is finally acceptable
        break;
    }

    case MemoryObject::TYPE_BYTE: {
        // Only allow AM_ZEROPAGE
        if(expression_mode != AM_ZEROPAGE) {
            stringstream ss;
            ss << "Expression must evaluate to a value between 0-255";
            errmsg = ss.str();
            return false;
        }

        // validate the word value is equal to the expression_value
        u8 operand = memory_object->data_ptr[0];
        if(operand != expression_value) {
            stringstream ss;
            ss << hex << setfill('0') << uppercase;
            ss << "Expression value ($" << setw(4) << (int)expression_value 
               << ") does not evaluate to data value ($" << setw(2) << operand << ")";
            errmsg = ss.str();
            return false;
        }
        break;
    }

    case MemoryObject::TYPE_WORD: {
        // Only allow AM_ABSOLUTE
        if(expression_mode != AM_ABSOLUTE) {
            stringstream ss;
            ss << "Expression addressing mode (" << magic_enum::enum_name(expression_mode) 
               << ") must be an absolute value";
            errmsg = ss.str();
            return false;
        }

        // validate the word value is equal to the expression_value
        u16 operand = (u16)memory_object->data_ptr[0] | ((u16)memory_object->data_ptr[1] << 8);
        if(operand != expression_value) {
            stringstream ss;
            ss << hex << setfill('0') << uppercase;
            ss << "Expression value ($" << setw(4) << (int)expression_value 
               << ") does not evaluate to data value ($" << setw(4) << operand << ")";
            errmsg = ss.str();
            return false;
        }
        break;
    }

    default:
        assert(false);
        break;
    }

    // save the expression and its value in the list of common expressions
    // we're only saving expressions that have 3 or more nodes so that
    // basic labels or constant usage doesn't get added to the set
    if(num_nodes >= 3) {
        // get the string representation of the expression
        stringstream ss;
        ss << *expr;
        string expression_string = ss.str();

        // add it to the set of the corresponding value
        auto& expression_set = quick_expressions_by_value[expression_value];
        auto res = expression_set.insert(expression_string);

        // only if the expression is new, emit the signal
        if(res.second) new_quick_expression->emit(expression_value, expression_string);
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
        FixupFlags fixup_flags, int* num_nodes)
{
    ExploreExpressionNodeData explore_data {
        .errmsg           = errmsg,
        .allow_modes      = (bool)(fixup_flags & FIXUP_ADDRESSING_MODES),
        .allow_labels     = (bool)(fixup_flags & FIXUP_LABELS),
        .allow_defines    = (bool)(fixup_flags & FIXUP_DEFINES),
        .allow_deref      = (bool)(fixup_flags & FIXUP_DEREFS),
        .long_mode_labels = (bool)(fixup_flags & FIXUP_LONG_LABELS),
        .allow_enums      = (bool)(fixup_flags & FIXUP_ENUMS)
    };

    // Loop over every node (and change them to system nodes if necessary), validating some things along the way
    auto cb = std::bind(&System::ExploreExpressionNodeCallback, this, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4);
    auto res = expr->Explore(cb, &explore_data);
    if(num_nodes) *num_nodes = explore_data.num_nodes;
    return res;
}

shared_ptr<Define> System::CreateDefine(string const& name, string& errmsg)
{
    int errloc;

    // evaluate 'name' and make sure we get a name node
    auto nameexpr = make_shared<Expression>();
    if(!nameexpr->Set(name, errmsg, errloc)) {
        errmsg = "Invalid name for Define";
        return nullptr;
    }

    auto define_node = dynamic_pointer_cast<BaseExpressionNodes::Name>(nameexpr->GetRoot());
    if(!define_node) {
        errmsg = "Invalid name for Define";
        return nullptr;
    }

    string define_name = define_node->GetString();

    // does define exist?
    if(auto other = defines.contains(define_name)) {
        errmsg = "Define name exists already";
        return nullptr;
    }


    // define looks good, add to database
    auto define = make_shared<Define>(define_name);
    defines[define_name] = define;

    // notify the system of new defines
    define_created->emit(define);

    return define;
}

bool System::DeleteDefine(shared_ptr<Define> const& define)
{
    if(define->GetNumReverseReferences()) {
        cout << "[Systems::NES::System] warning: deleting define with nonzero RRefs" << endl;
    }

    defines.erase(define->GetName());
    define_deleted->emit(define);
    define->ClearReferences();

    return true;
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


vector<shared_ptr<Label>> const& System::GetLabelsAt(GlobalMemoryLocation const& where)
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

void System::DeleteLabel(shared_ptr<Label> const& label)
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

shared_ptr<Enum> System::CreateEnum(string const& name)
{
    if(enums.contains(name)) return nullptr;

    auto e = make_shared<Enum>(name);
    enums[name] = e;
    *e->element_added   += quick_bind(&System::EnumElementAdded  , this);
    *e->element_changed += quick_bind(&System::EnumElementChanged, this);
    *e->element_deleted += quick_bind(&System::EnumElementDeleted, this);

    enum_created->emit(e);

    return e; 
}

shared_ptr<Enum> const& System::GetEnum(string const& name)
{
    static shared_ptr<Enum> null_enum;
    if(enums.contains(name)) return enums[name];
    return null_enum; 
}

shared_ptr<EnumElement> const& System::GetEnumElement(string const& name)
{
    static shared_ptr<EnumElement> null_ee;
    if(enum_elements_by_name.contains(name)) return enum_elements_by_name[name];
    return null_ee; 
}

bool System::DeleteEnum(shared_ptr<Enum> const& e)
{
    enum_deleted->emit(e);
    e->DeleteElements();
    assert(enums.contains(e->GetName()));
    enums.erase(e->GetName());
    return true;
}

void System::EnumElementAdded(shared_ptr<EnumElement> const& ee)
{
    auto& list = enum_elements_by_value[ee->cached_value];
    list.push_back(ee);

    auto e = ee->parent_enum.lock();
    assert(e);

    enum_elements_by_name[ee->GetFormattedName("_")]  = ee;

    enum_element_added->emit(ee);
}

void System::EnumElementChanged(shared_ptr<EnumElement> const& ee, std::string const& old_name, s64 old_value)
{
    if(ee->GetName() != old_name) {
        auto e = ee->parent_enum.lock();
        enum_elements_by_name.erase(e->GetName() + "_" + old_name);

        enum_elements_by_name[ee->GetFormattedName("_")] = ee;
    }

    if(ee->cached_value != old_value) {
        assert(enum_elements_by_value.contains(old_value));
        auto& list = enum_elements_by_value[old_value];
        auto it = find(list.begin(), list.end(), ee);
        assert(it != list.end());
        list.erase(it);

        auto& list2 = enum_elements_by_value[ee->cached_value];
        list2.push_back(ee);
    }

    enum_element_changed->emit(ee, old_value);
}

void System::EnumElementDeleted(shared_ptr<EnumElement> const& ee)
{
    enum_elements_by_name.erase(ee->GetFormattedName("_"));
    assert(enum_elements_by_value.contains(ee->cached_value));
    auto& list = enum_elements_by_value[ee->cached_value];
    auto it = find(list.begin(), list.end(), ee);
    assert(it != list.end());
    list.erase(it);

    enum_element_deleted->emit(ee);
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

            u8 op = *memory_object->data_ptr;
            string inst = disassembler->GetInstruction(op);

            // stop disassembling on unknown opcodes
            int size = disassembler->GetInstructionSize(op);
            if(size == 0) {
                cout << "[NES::System::DisassemblyThread] stopping because invalid opcode $" << hex << uppercase << (int)*memory_object->data_ptr << " (" << inst << ") at "  << current_loc << endl;
                break; // break on unimplemented opcodes (TODO remove me)
            }

            // convert the memory to code
            if(!memory_region->MarkMemoryAsCode(current_loc)) {
                assert(false); // this shouldn't happen
                break;
            }

            // re-get the memory object JIC
            memory_object = memory_region->GetMemoryObject(current_loc);

            // create the expressions as necessary
            auto det_func = [](u32, finish_default_operand_expression_func finish_expression) { 
                finish_expression(nullopt); // during automated disassembly, we can't ask the user for bank selection
            };
            CreateDefaultOperandExpression(current_loc, true, det_func);

            // certain instructions must stop disassembly and others cause branches
            switch(op) {
            case 0x4C: // JMP absolute
                disassembling_inner = false;
                // fall through
            case 0x20: // JSR absolute
            {
                u16 target = (u16)memory_object->data_ptr[1] | ((u16)memory_object->data_ptr[2] << 8);
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
                u16 target = (u16)((s16)(current_loc.address + 2) + (s16)(s8)memory_object->data_ptr[1]);

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

void System::CreateDefaultOperandExpression(GlobalMemoryLocation const& where, bool with_labels,
                                            determine_memory_region_func det_func)
{
    auto code_region = GetMemoryRegion(where);
    auto code_object = GetMemoryObject(where);

    // only create operand expressions for code
    if(code_object->type != MemoryObject::TYPE_CODE) return;

    switch(auto am = disassembler->GetAddressingMode(*code_object->data_ptr)) {
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

        u16 target = isrel ? (u16)((s16)(where.address + 2) + (s16)(s8)code_object->data_ptr[1])
                           : (is16 ? ((u16)code_object->data_ptr[1] | ((u16)code_object->data_ptr[2] << 8))
                                   : (u16)code_object->data_ptr[1]);

        auto finish_expression = [=](std::optional<GlobalMemoryLocation> const& target_location)->void {
            // only for valid destination addresses do we create a label
            // can't call GetDefaultLabelForTarget if is_valid is false because target_location might still be
            // set up to point to something in the wrong bank
            int target_offset = 0;
            bool wide = !(target < 0x100);
            string prefix = isrel ? "." : (wide ? "L_" : "zp_");
            shared_ptr<Label> label = (with_labels && target_location) ? GetDefaultLabelForTarget(*target_location, false, &target_offset, wide, prefix) : nullptr;

            // now create an expression for the operands
            auto expr = make_shared<Expression>();
            auto nc = dynamic_pointer_cast<ExpressionNodeCreator>(expr->GetNodeCreator());

            // format the operand label display string
            char buf[6];
            if(is16 || isrel) {
                snprintf(buf, sizeof(buf), "$%04X", target);
            } else {
                snprintf(buf, sizeof(buf), "$%02X", target);
            }

            // use a label node if we know the label exist
            auto root = label ? nc->CreateLabel(*target_location, 0, buf)
                              : nc->CreateConstant(target, buf);

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
        };

        GlobalMemoryLocation target_location;
        target_location.address = target;

        // if the target is not in a bankable region, use the address directly
        // if the target is in the same bank, copy over that bank number
        // if the target is in a banked region, try to determine the bank
        if(target >= code_region->GetBaseAddress() && target < code_region->GetEndAddress()) {
            target_location.prg_rom_bank = where.prg_rom_bank;
            finish_expression(target_location);
        } else if(CanBank(target_location)) {
            vector<u16> banks;
            GetBanksForAddress(target_location, banks);
            if(banks.size() == 1) {
                target_location.prg_rom_bank = banks[0];
                finish_expression(target_location);
            } else {
                // here we can't always ask the user for which bank, since we could be disassembling
                det_func(target, finish_expression);
            }
        } else { // !CanBank
            // the label is only applied if the target location is valid
            if(GetMemoryObject(target_location)) finish_expression(target_location);
            else                                 finish_expression(nullopt);
        }

        break;
    }

    case AM_IMMEDIATE: // Immediate instructions don't get labels
    {
        u8 imm = code_object->data_ptr[1];

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

bool System::Save(ostream& os, string& errmsg)
{
    // save the enums before defines, as defines can reference enums and they need to
    // be loaded before defines in Load()
    WriteVarInt(os, enums.size());
    if(!os.good()) goto done;
    for(auto& e : enums) if(!e.second->Save(os, errmsg)) return false;

    // save the defines
    WriteVarInt(os, defines.size());
    if(!os.good()) goto done;
    for(auto& define : defines) if(!define.second->Save(os, errmsg)) return false;

    // save the labels globally, as parsing expressions in memory objects that use labels
    // will need to be able to look them up at load.
    WriteVarInt(os, label_database.size());
    if(!os.good()) goto done;
    for(auto& label : label_database) if(!label.second->Save(os, errmsg)) return false;

    // save the non-cartridge memory regions
    if(!cpu_ram->Save(os, errmsg)) return false;
    if(!ppu_registers->Save(os, errmsg)) return false;
    if(!io_registers->Save(os, errmsg)) return false;

    // save the cartridge
    if(!cartridge->Save(os, errmsg)) return false;

    // save the quick expressions
    WriteVarInt(os, (int)quick_expressions_by_value.size());
    for(auto& qe: quick_expressions_by_value) {
        WriteVarInt(os, qe.first);
        WriteVarInt(os, (int)qe.second.size());
        for(auto qe_string: qe.second) {
            WriteString(os, qe_string);
        }
    }

done:
    errmsg = "Error saving System";
    return os.good();
}

bool System::Load(istream& is, string& errmsg)
{
    int num_defines = 0;
    int num_labels = 0;

    shared_ptr<BaseSystem> base_system = shared_from_this();
    auto selfptr = dynamic_pointer_cast<System>(base_system);
    assert(selfptr);

    // load enums
    if(GetCurrentProject()->GetSaveFileVersion() >= FILE_VERSION_ENUMS) {
        int num_enums = ReadVarInt<int>(is);
        if(!is.good()) goto done;

        for(int i = 0; i < num_enums; i++) {
            auto e = Enum::Load(is, errmsg);
            if(!e) return false;

            enums[e->GetName()] = e;

            // iterate over elements and add them to our value map
            e->IterateElements([this](shared_ptr<EnumElement> const& ee) {
                enum_elements_by_value[ee->cached_value].push_back(ee);
                enum_elements_by_name[ee->GetFormattedName("_")] = ee;
            });

            // connect to the enum signals
            *e->element_added   += quick_bind(&System::EnumElementAdded  , this);
            *e->element_changed += quick_bind(&System::EnumElementChanged, this);
            *e->element_deleted += quick_bind(&System::EnumElementDeleted, this);
        }
    }

    // load defines
    num_defines = ReadVarInt<int>(is);
    if(!is.good()) goto done;

    for(int i = 0; i < num_defines; i++) {
        auto define = Define::Load(is, errmsg);
        if(!define) return false;

        define->NoteReferences();
        defines[define->GetName()] = define;
    }

    cout << "[System::Load] loaded " << num_defines << " defines." << endl;

    // load labels
    num_labels = ReadVarInt<int>(is);
    if(!is.good()) goto done;

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

    // load the quick expressions
    if(GetCurrentProject()->GetSaveFileVersion() >= FILE_VERSION_QUICKEXP) {
        int num_quick_expressions_values = ReadVarInt<int>(is);
        for(int i = 0; i < num_quick_expressions_values; i++) {
            int quick_expressions_value = ReadVarInt<s64>(is);
            int num_quick_expressions = ReadVarInt<int>(is);
            auto& quick_expressions = quick_expressions_by_value[quick_expressions_value];
            for(int j = 0; j < num_quick_expressions; j++) {
                string s;
                ReadString(is, s);
                if(!is.good()) goto done;
                quick_expressions.insert(s);
            }
        }
    }

    // note all references
    NoteReferences();

done:
    errmsg = "Error loading System";
    return is.good();
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
