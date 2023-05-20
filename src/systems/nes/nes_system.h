#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "systems/system.h"


#include "systems/nes/nes_listing.h"
#include "systems/nes/nes_defs.h"
#include "systems/nes/nes_memory.h"

class BaseExpressionNode;

namespace NES {

class Cartridge;
class Define;
class Disassembler;
class Expression;
class ExpressionNodeCreator;
class Label;
class ProgramRomBank;

class System : public ::BaseSystem {
public:
    System();
    virtual ~System();

    // Signals
    typedef signal<std::function<void(std::shared_ptr<Define> const&)>> define_created_t;
    std::shared_ptr<define_created_t> define_created;

    typedef signal<std::function<void(std::shared_ptr<Label> const&, bool)>> label_created_t;
    std::shared_ptr<label_created_t> label_created;

    // On-demand new signal handlers for specific addresses
    std::shared_ptr<label_created_t> LabelCreatedAt(GlobalMemoryLocation const& where) {
        if(!label_created_at.contains(where)) {
            label_created_at[where] = std::make_shared<label_created_t>();
        }
        return label_created_at[where];
    }

    // Be polite and tell me when you disconnect
    void LabelCreatedAtRemoved(GlobalMemoryLocation const& where) {
        if(label_created_at.contains(where)) {
            if(label_created_at[where]->connections.size() == 0) {
                label_created_at.erase(where);
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

    void MarkMemoryAsUndefined(GlobalMemoryLocation const&);
    void MarkMemoryAsWords(GlobalMemoryLocation const&, u32 byte_count);
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

    // Listings
    void GetListingItems(GlobalMemoryLocation const&, std::vector<std::shared_ptr<NES::ListingItem>>& out);

    // Defines
    void CreateDefaultDefines(); // for new projects
    std::shared_ptr<Define> AddDefine(std::string const& name, std::string const&, std::string& errmsg);
    std::shared_ptr<Define> FindDefine(std::string const& name) {
        if(define_by_name.contains(name)) return define_by_name[name];
        return nullptr;
    }

    template <typename F>
    void IterateDefines(F const* cb) {
        for(auto iter : define_by_name) {
            std::shared_ptr<Define> define = iter.second;
            (*cb)(define);
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

    template <typename F>
    void IterateLabels(F const* cb) {
        for(auto iter : label_database) {
            std::shared_ptr<Label> label = iter.second;
            (*cb)(label);
        }
    }
    
    std::shared_ptr<Label> GetOrCreateLabel(GlobalMemoryLocation const&, std::string const&, bool was_user_created = false);
    std::shared_ptr<Label> CreateLabel(GlobalMemoryLocation const&, std::string const&, bool was_user_created = false);
    std::shared_ptr<Label> EditLabel(GlobalMemoryLocation const&, std::string const&, int nth, bool was_user_edited = false);
    void                   InsertLabel(std::shared_ptr<Label>&);

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
    void CreateDefaultOperandExpression(GlobalMemoryLocation const&);

    //!std::shared_ptr<LabelList> GetLabels(GlobalMemoryLocation const& where) {
    //!    if(!label_database.contains(where)) return nullptr;
    //!    return label_database[where];
    //!}

    // Save and load
    bool Save(std::ostream&, std::string&) override;
    bool Load(std::istream&, std::string&) override;

private:
    std::unordered_map<GlobalMemoryLocation, std::shared_ptr<label_created_t>> label_created_at;

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

        std::vector<std::string> undefined_names; // All other Names that were not labels or defines
    };

    std::shared_ptr<ExpressionNodeCreator> GetNodeCreator();
    bool ExploreExpressionNodeCallback(std::shared_ptr<BaseExpressionNode>&, std::shared_ptr<BaseExpressionNode> const&, int, void*);
	bool DetermineAddressingMode(std::shared_ptr<Expression>&, ADDRESSING_MODE*, s64*, std::string&);
};

}


