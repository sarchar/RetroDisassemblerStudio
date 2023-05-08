#pragma once

#include "systems/system.h"

#include "systems/nes/nes_content.h"
#include "systems/nes/nes_defs.h"
#include "systems/nes/nes_memory.h"

class NESSystem; // TODO move into NES namespace

namespace NES {

class Cartridge;
class ProgramRomBank;

class System : public ::BaseSystem {
public:
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

    // Content
    std::shared_ptr<ContentBlock>  GetContentBlockAt(GlobalMemoryLocation const&);
    void MarkContentAsData(NES::GlobalMemoryLocation const&, u32 byte_count, NES::CONTENT_BLOCK_DATA_TYPE data_type);

    // Listings
    void GetListingItems(NES::GlobalMemoryLocation const&, std::vector<std::shared_ptr<NES::ListingItem>>& out);

    // Labels
    void CreateLabel(GlobalMemoryLocation const&, std::string const&);

private:
    std::string rom_file_path_name;
};

}


