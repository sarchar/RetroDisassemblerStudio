#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "systems/system.h"

#include "systems/nes/nes_listing.h"
#include "systems/nes/nes_defs.h"
#include "systems/nes/nes_memory.h"

class NESSystem; // TODO move into NES namespace

namespace NES {

class Cartridge;
class Disassembler;
class Label;
class ProgramRomBank;

class System : public ::BaseSystem {
public:
    System();
    virtual ~System();

    // Signals
    typedef signal<std::function<void(std::shared_ptr<Label> const&, bool)>> label_created_t;
    std::shared_ptr<label_created_t> label_created;

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
    std::shared_ptr<MemoryObject> GetMemoryObject(GlobalMemoryLocation const&);

    void MarkMemoryAsUndefined(GlobalMemoryLocation const&);
    void MarkMemoryAsWords(GlobalMemoryLocation const&, u32 byte_count);

    // Listings
    void GetListingItems(GlobalMemoryLocation const&, std::vector<std::shared_ptr<NES::ListingItem>>& out);

    // Labels
    void CreateDefaultLabels(); // for new projects

    std::vector<std::shared_ptr<Label>> const& GetLabelsAt(GlobalMemoryLocation const&);

    std::shared_ptr<Label> FindLabel(std::string const& label_str) {
        if(label_database.contains(label_str)) return label_database[label_str];
        return nullptr;
    }
    
    std::shared_ptr<Label> GetOrCreateLabel(GlobalMemoryLocation const&, std::string const&, bool was_user_created = false);
    std::shared_ptr<Label> CreateLabel(GlobalMemoryLocation const&, std::string const&, bool was_user_created = false);
    std::shared_ptr<Label> EditLabel(GlobalMemoryLocation const&, std::string const&, int nth, bool was_user_edited = false);

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
    // Memory
    std::shared_ptr<NES::PPURegistersRegion> ppu_registers;
    std::shared_ptr<NES::IORegistersRegion> io_registers;
    std::shared_ptr<NES::Cartridge> cartridge;

    // label database
    std::unordered_map<std::string, std::shared_ptr<Label>> label_database = {};
    //!std::unordered_map<GlobalMemoryLocation, std::shared_ptr<LabelList>, GlobalMemoryLocation::HashFunction> label_database = {};

    bool disassembling;
    GlobalMemoryLocation disassembly_address;

    std::shared_ptr<Disassembler> disassembler;
};

}


