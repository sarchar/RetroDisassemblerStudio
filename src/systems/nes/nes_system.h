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
class ProgramRomBank;

class System : public ::BaseSystem {
public:
    typedef std::vector<std::string> LabelList;

    System();
    virtual ~System();

    System::Information const* GetInformation();
    bool CreateNewProjectFromFile(std::string const&);

    // creation interface
    static System::Information const* GetInformationStatic();
    static bool IsROMValid(std::string const& file_path_name, std::istream& is);
    static std::shared_ptr<BaseSystem> CreateSystem();

    std::shared_ptr<NES::Cartridge>& GetCartridge() { return cartridge; }
    std::shared_ptr<NES::Cartridge> cartridge;

    // Memory
    void GetEntryPoint(NES::GlobalMemoryLocation*);
    u16  GetMemoryRegionBaseAddress(GlobalMemoryLocation const&);
    u32  GetMemoryRegionSize(GlobalMemoryLocation const&);

    std::shared_ptr<MemoryRegion> GetMemoryRegion(GlobalMemoryLocation const&);

//!    // Content
//!    void MarkContentAsData(NES::GlobalMemoryLocation const&, u32 byte_count, NES::CONTENT_BLOCK_DATA_TYPE data_type);

    // Listings
    void GetListingItems(NES::GlobalMemoryLocation const&, std::vector<std::shared_ptr<NES::ListingItem>>& out);

    // Labels
    void CreateLabel(GlobalMemoryLocation const&, std::string const&);

    //!std::shared_ptr<LabelList> GetLabels(GlobalMemoryLocation const& where) {
    //!    if(!label_database.contains(where)) return nullptr;
    //!    return label_database[where];
    //!}

private:
    std::string rom_file_path_name;

    // label database
    //std::unordered_map<GlobalMemoryLocation, std::shared_ptr<LabelList>, GlobalMemoryLocation::HashFunction> label_database = {};
};

}


