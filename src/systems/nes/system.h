// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "systems/system.h"

#include "systems/nes/defs.h"
#include "systems/nes/memory.h"

class BaseExpression;
class BaseExpressionNode;

namespace Systems::NES {

class Cartridge;
class CartridgeView;
class Define;
class Disassembler;
class Expression;
class ExpressionNodeCreator;
class Label;
class ProgramRomBank;
class SystemView;

class System : public ::BaseSystem {
public:
    System();
    virtual ~System();

    // Signals
    typedef signal<std::function<void(std::shared_ptr<Define> const&)>> define_created_t;
    std::shared_ptr<define_created_t> define_created;

    typedef signal<std::function<void(std::shared_ptr<Label> const&, bool)>> label_created_t;
    std::shared_ptr<label_created_t> label_created;

    typedef signal<std::function<void(std::shared_ptr<Label> const&, int)>> label_deleted_t;
    std::shared_ptr<label_deleted_t> label_deleted;

    // On-demand new signal handlers for specific addresses
    std::shared_ptr<label_created_t> LabelCreatedAt(GlobalMemoryLocation const& where) {
        if(!label_created_at.contains(where)) {
            label_created_at[where] = std::make_shared<label_created_t>();
        }
        return label_created_at[where];
    }

    std::shared_ptr<label_deleted_t> LabelDeletedAt(GlobalMemoryLocation const& where) {
        if(!label_deleted_at.contains(where)) {
            label_deleted_at[where] = std::make_shared<label_deleted_t>();
        }
        return label_deleted_at[where];
    }

    // Be polite and tell me when you disconnect
    void LabelCreatedAtRemoved(GlobalMemoryLocation const& where) {
        if(label_created_at.contains(where)) {
            if(label_created_at[where]->connections.size() == 0) {
                label_created_at.erase(where);
            }
        }
    }

    void LabelDeletedAtRemoved(GlobalMemoryLocation const& where) {
        if(label_deleted_at.contains(where)) {
            if(label_deleted_at[where]->connections.size() == 0) {
                label_deleted_at.erase(where);
            }
        }
    }

    typedef signal<std::function<void(GlobalMemoryLocation const&)>> disassembly_stopped_t;
    std::shared_ptr<disassembly_stopped_t> disassembly_stopped;

    // Cartridge
    std::shared_ptr<NES::Cartridge>& GetCartridge() { return cartridge; }

    // Memory
    void CreateMemoryRegions();
    void GetEntryPoint(NES::GlobalMemoryLocation*);
    bool CanBank(GlobalMemoryLocation const&);
    void GetBanksForAddress(GlobalMemoryLocation const&, std::vector<u16>&);
    int GetNumMemoryRegions() const;
    std::shared_ptr<MemoryRegion> GetMemoryRegion(GlobalMemoryLocation const&);
    std::shared_ptr<MemoryRegion> GetMemoryRegionByIndex(int);
    std::shared_ptr<MemoryObject> GetMemoryObject(GlobalMemoryLocation const&, int* offset = NULL);

    void MarkMemoryAsUndefined(GlobalMemoryLocation const&, u32 byte_count);
    void MarkMemoryAsWords(GlobalMemoryLocation const&, u32 byte_count);
    void MarkMemoryAsString(GlobalMemoryLocation const&, u32 byte_count);

    // Convert units like Names into defines and labels 
    // TODO this is now silly enough that the bools should be a single flag parameter instead
    bool FixupExpression(std::shared_ptr<BaseExpression> const&, std::string&,
            bool allow_labels = true,
            bool allow_defines = true,
            bool allow_deref = true,
            bool allow_addressing_modes = false,
            bool long_mode_labels = false);

    // Set the operand_expression at a memory location. System::SetOperandExpression performs a FixupExpression and
    // checks the addressing mode. Calling MemoryRegion::SetOperandExpression bypasses this
    bool SetOperandExpression(GlobalMemoryLocation const&, std::shared_ptr<Expression>&, std::string& errmsg);


    int GetSortableMemoryLocation(GlobalMemoryLocation const& s) {
        int ret = s.address;
        if(CanBank(s)) {
            int bank = s.prg_rom_bank;
            if(s.is_chr) {
                ret += 0x01000000;
                bank = s.chr_rom_bank;
            }
            ret += 0x010000 * bank;
        }
        return ret;
    }

    std::shared_ptr<MemoryView> CreateMemoryView(std::shared_ptr<MemoryView> const& ppu_view, std::shared_ptr<MemoryView> const& apu_io_view);

    // Defines
    void CreateDefaultDefines(); // for new projects
    std::shared_ptr<Define> AddDefine(std::string const& name, std::string const&, std::string& errmsg);
    std::shared_ptr<Define> FindDefine(std::string const& name) {
        if(define_by_name.contains(name)) return define_by_name[name];
        return nullptr;
    }

    template <typename F>
    void IterateDefines(F cb) {
        for(auto iter : define_by_name) {
            std::shared_ptr<Define> define = iter.second;
            cb(define);
        }
    }

    // Labels
    void CreateDefaultLabels(); // for new projects

    std::shared_ptr<Label> GetDefaultLabelForTarget(GlobalMemoryLocation const& where, bool was_user_created, int* offset = nullptr, bool wide = true, std::string const& prefix = "L_");
    std::vector<std::shared_ptr<Label>> const& GetLabelsAt(GlobalMemoryLocation const&);

    std::shared_ptr<Label> FindLabel(std::string const& label_str) {
        if(label_database.contains(label_str)) return label_database[label_str];
        return nullptr;
    }

    void DeleteLabel(std::shared_ptr<Label> const&);

    template <typename F>
    void IterateLabels(F cb) {
        for(auto iter : label_database) {
            std::shared_ptr<Label> label = iter.second;
            cb(label);
        }
    }
    
    std::shared_ptr<Label> GetOrCreateLabel(GlobalMemoryLocation const&, std::string const&, bool was_user_created = false);
    std::shared_ptr<Label> CreateLabel(GlobalMemoryLocation const&, std::string const&, bool was_user_created = false);
    std::shared_ptr<Label> EditLabel(GlobalMemoryLocation const&, std::string const&, int nth, bool was_user_edited = false);

    // Comments
    void GetComment(GlobalMemoryLocation const& where, MemoryObject::COMMENT_TYPE type, std::string& out) {
        if(auto memory_region = GetMemoryRegion(where)) {
            memory_region->GetComment(where, type, out);
        }
    }

    void SetComment(GlobalMemoryLocation const& where, MemoryObject::COMMENT_TYPE type, std::string const& comment) {
        if(auto memory_region = GetMemoryRegion(where)) {
            memory_region->SetComment(where, type, comment);
        }
    }

    // Disassembly
    std::shared_ptr<Disassembler> GetDisassembler() { return disassembler; }
    bool IsDisassembling() const { return disassembling; }
    void InitDisassembly(GlobalMemoryLocation const&);
    int  DisassemblyThread();
    void CreateDefaultOperandExpression(GlobalMemoryLocation const&, bool with_labels);

    //!std::shared_ptr<LabelList> GetLabels(GlobalMemoryLocation const& where) {
    //!    if(!label_database.contains(where)) return nullptr;
    //!    return label_database[where];
    //!}

    // Save and load
    bool Save(std::ostream&, std::string&) override;
    bool Load(std::istream&, std::string&) override;

private:
    std::unordered_map<GlobalMemoryLocation, std::shared_ptr<label_created_t>> label_created_at;
    std::unordered_map<GlobalMemoryLocation, std::shared_ptr<label_deleted_t>> label_deleted_at;

    // Memory
    std::shared_ptr<NES::RAMRegion> cpu_ram;
    std::shared_ptr<NES::PPURegistersRegion> ppu_registers;
    std::shared_ptr<NES::IORegistersRegion> io_registers;
    std::shared_ptr<NES::Cartridge> cartridge;

    // label database
    std::unordered_map<std::string, std::shared_ptr<Label>> label_database = {};

    // defines database
    std::vector<std::shared_ptr<Define>> defines = {};
    std::unordered_map<std::string, std::shared_ptr<Define>> define_by_name = {};

    void NoteReferences();

    bool disassembling;
    GlobalMemoryLocation disassembly_address;

    std::shared_ptr<Disassembler> disassembler;

    // Userdata passed to ExploreExpressionNodeCallback
    struct ExploreExpressionNodeData {
        std::string& errmsg; // any error generated sets an error message

        bool allow_modes;    // true if the explore can change syntax into CPU addressing modes

        bool allow_labels;   // allow looking up Labels
        std::vector<std::shared_ptr<Label>> labels;

        bool allow_defines;   // allow looking up Defines
        std::vector<std::shared_ptr<Define>> defines;

        bool allow_deref;    // allow dereference nodes
        std::vector<std::string> undefined_names; // All other Names that were not labels or defines

        bool long_mode_labels; // call SetLongMode(true) on all labels
    };

    std::shared_ptr<ExpressionNodeCreator> GetNodeCreator();
    bool ExploreExpressionNodeCallback(std::shared_ptr<BaseExpressionNode>&, std::shared_ptr<BaseExpressionNode> const&, int, void*);
	bool DetermineAddressingMode(std::shared_ptr<Expression>&, ADDRESSING_MODE*, s64*, std::string&);

    friend class SystemView;
};

class SystemView : public MemoryView {
public:
    SystemView(std::shared_ptr<BaseSystem> const&, 
            std::shared_ptr<MemoryView> const& _ppu_view, std::shared_ptr<MemoryView> const& _apu_io_view);
    virtual ~SystemView();

    u8 Peek(u16) override;
    u8 Read(u16) override;
    void Write(u16, u8) override;

    u8 PeekPPU(u16) override;
    u8 ReadPPU(u16) override;
    void WritePPU(u16, u8) override;

    void CopyVRAM(u8* dest, u16 offset = 0, u16 size = 0x800) {
        assert(offset < 0x800);
        int left = 0x800 - offset;
        memcpy(dest, &VRAM[offset], (left < size) ? left : size);
    }

    std::shared_ptr<CartridgeView> const& GetCartridgeView() const { return cartridge_view; }
private:
    std::shared_ptr<System> system;
    std::shared_ptr<MemoryView> ppu_view;
    std::shared_ptr<MemoryView> apu_io_view;
    std::shared_ptr<CartridgeView> cartridge_view;

    // It could be more C++ish by using RAMRegion to request a memory view and redirect
    // read/writes there, but RAM is so simple I think I'll just embed it directly into SystemView.
    u8 RAM[0x800];
    u8 VRAM[0x800];
};

}


