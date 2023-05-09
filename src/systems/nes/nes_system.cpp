#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>

#include "systems/nes/nes_cartridge.h"
#include "systems/nes/nes_disasm.h"
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
}

System::~System()
{
}

bool System::CreateNewProjectFromFile(string const& file_path_name)
{
    cout << "[NES::System] CreateNewProjectFromFile begin" << endl;
    create_new_project_progress->emit(shared_from_this(), false, 0, 0, "Loading file...");

    shared_ptr<BaseSystem> base_system = shared_from_this();
    auto selfptr = dynamic_pointer_cast<System>(base_system);
    assert(selfptr);
    cartridge = make_shared<Cartridge>(selfptr);

    // Read in the iNES header
    ifstream rom_stream(file_path_name, ios::binary);
    if(!rom_stream) {
        create_new_project_progress->emit(shared_from_this(), true, 0, 0, "Error: Could not open file");
        return false;
    }

    unsigned char buf[16];
    rom_stream.read(reinterpret_cast<char*>(buf), 16);
    if(!rom_stream || !(buf[0] == 'N' && buf[1] == 'E' && buf[2] == 'S' && buf[3] == 0x1A)) {
        create_new_project_progress->emit(shared_from_this(), true, 0, 0, "Error: Not an NES ROM file");
        return false;
    }
 
    // Parse the iNES header
    cartridge->header.num_prg_rom_banks = (u8)buf[4];
    cartridge->header.num_chr_rom_banks = (u8)buf[5];
    cartridge->header.prg_rom_size      = cartridge->header.num_prg_rom_banks * 16 * 1024;
    cartridge->header.chr_rom_size      = cartridge->header.num_chr_rom_banks *  8 * 1024;
    cartridge->header.mapper            = ((u8)buf[6] & 0xF0) >> 4 | ((u8)buf[7] & 0xF0);
    cartridge->header.mirroring         = ((u8)buf[6] & 0x08) ? MIRRORING_FOUR_SCREEN : ((bool)((u8)buf[6] & 0x01) ? MIRRORING_VERTICAL : MIRRORING_HORIZONTAL);
    cartridge->header.has_sram          = (bool)((u8)buf[6] & 0x02);
    cartridge->header.has_trainer       = (bool)((u8)buf[6] & 0x04);

    // Allocate storage and initialize cart based on header information
    cartridge->Prepare();

    // skip the trainer if present
    if(cartridge->header.has_trainer) rom_stream.seekg(512, rom_stream.cur);

    // we now know how many things we need to load
    u32 num_steps = cartridge->header.num_prg_rom_banks + cartridge->header.num_chr_rom_banks + 1;
    u32 current_step = 0;

    // Load the PRG banks
    for(u32 i = 0; i < cartridge->header.num_prg_rom_banks; i++) {
        stringstream ss;
        ss << "Loading PRG ROM bank " << i;
        create_new_project_progress->emit(shared_from_this(), false, num_steps, ++current_step, ss.str());

        // Read in the PRG rom data
        unsigned char data[16 * 1024];
        cout << rom_stream.tellg() << endl;
        rom_stream.read(reinterpret_cast<char *>(data), sizeof(data));
        if(!rom_stream) {
            create_new_project_progress->emit(shared_from_this(), true, num_steps, current_step, "Error: file too short when reading PRG-ROM");
            return false;
        }

        // Get the PRG-ROM bank
        auto prg_bank = cartridge->GetProgramRomBank(i); // Bank should be empty with no content

        // Initialize the entire bank as just a series of bytes
        prg_bank->InitializeFromData(reinterpret_cast<u8*>(data), sizeof(data)); // Initialize the memory region with bytes of data
    }

    // Load the CHR banks
    for(u32 i = 0; i < cartridge->header.num_chr_rom_banks; i++) {
        stringstream ss;
        ss << "Loading CHR ROM bank " << i;
        create_new_project_progress->emit(shared_from_this(), false, num_steps, ++current_step, ss.str());

        // Read in the CHR rom data
        unsigned char data[8 * 1024]; // TODO read 4K banks with other mappers (check chr_bank first!)
        rom_stream.read(reinterpret_cast<char *>(data), sizeof(data));
        if(!rom_stream) {
            create_new_project_progress->emit(shared_from_this(), true, num_steps, current_step, "Error: file too short when reading CHR-ROM");
            return false;
        }

        // Get the CHR bank
        auto chr_bank = cartridge->GetCharacterRomBank(i); // Bank should be empty with no content

        // Initialize the entire bank as just a series of bytes
        chr_bank->InitializeFromData(reinterpret_cast<u8*>(data), sizeof(data)); // Mark content starting at offset 0 as data
    }

    // Create the CPU vector labels
    GlobalMemoryLocation vectors;
    GetEntryPoint(&vectors);
    CreateLabel(vectors, "_reset");

    vectors.address -= 2;
    CreateLabel(vectors, "_nmi");

    vectors.address += 4;
    CreateLabel(vectors, "_irqbrk");

    create_new_project_progress->emit(shared_from_this(), false, num_steps, ++current_step, "Done");
    std::this_thread::sleep_for(std::chrono::seconds(1));

    cout << "[NES::System] CreateNewProjectFromFile end" << endl;
    return true;
}

bool System::IsROMValid(std::string const& file_path_name, std::istream& is)
{
    unsigned char buf[16];
    is.read(reinterpret_cast<char*>(buf), 16);
    if(is && (buf[0] == 'N' && buf[1] == 'E' && buf[2] == 'S' && buf[3] == 0x1A)) {
        return true;
    }

    return false;
}

// Memory
void System::GetEntryPoint(GlobalMemoryLocation* out)
{
    assert((bool)cartridge);
    zero(out);
    out->address      = 0xFFFC;
    out->prg_rom_bank = cartridge->GetResetVectorBank();
}

u16 System::GetMemoryRegionBaseAddress(GlobalMemoryLocation const& where)
{
    assert(!where.is_chr); // TODO

    if(where.address < 0x2000) {
        return where.address & ~0x7FF;
    } else if(where.address < 0x4020) {
        return 0x2000;
    } else if(where.address < 0x6000) {
        return 0x4020;
    } else if(where.address < 0x8000) {
        return 0x6000;
    } else {
        auto prg_bank = cartridge->GetProgramRomBank(where.prg_rom_bank);
        return prg_bank->GetBaseAddress();
    }
}

u32 System::GetMemoryRegionSize(GlobalMemoryLocation const& where)
{
    assert(!where.is_chr); // TODO

    if(where.address < 0x2000) {
        return 0x800;
    } else if(where.address < 0x4020) {
        return 0x2020;
    } else if(where.address < 0x6000) {
        return 0x1FE0;
    } else if(where.address < 0x8000) {
        return 0x2000;
    } else {
        auto prg_bank = cartridge->GetProgramRomBank(where.prg_rom_bank);
        return prg_bank->GetRegionSize();
    }
}

std::shared_ptr<MemoryRegion> System::GetMemoryRegion(GlobalMemoryLocation const& where)
{
    static shared_ptr<MemoryRegion> empty_ptr;
    assert(!where.is_chr); // TODO

    if(where.address < 0x2000) {
        return empty_ptr;
    } else if(where.address < 0x4020) {
        return empty_ptr;
    } else if(where.address < 0x6000) {
        return empty_ptr;
    } else if(where.address < 0x8000) {
        return empty_ptr;
    } else {
        return cartridge->GetProgramRomBank(where.prg_rom_bank);
    }
}

void System::MarkMemoryAsWords(GlobalMemoryLocation const& where, u32 byte_count)
{
    auto memory_region = GetMemoryRegion(where);
    memory_region->MarkMemoryAsWords(where, byte_count);
}

void System::CreateLabel(GlobalMemoryLocation const& where, string const& label)
{
    auto memory_region = GetMemoryRegion(where);
    memory_region->CreateLabel(where, label);

//!    LabelList ll = label_database[where];
//!    if(!ll) {
//!        ll = make_shared<LabelList>();
//!        label_database[where] = ll;
//!    }
//!
//!    ll->append(label);
//!
//!    cout << "defined label " << label << " at " << where << endl;
}

void System::BeginDisassembly(GlobalMemoryLocation const& where)
{
    disassembly_address = where;

    disassembling = true;
}

int System::DisassemblyThread()
{
    auto memory_region = GetMemoryRegion(disassembly_address);

    while(true) {
        // give up if we can't even convert this data to code. the user must clear the data type first
        auto memory_object = memory_region->GetMemoryObject(disassembly_address);
        if(memory_object->type != MemoryObject::TYPE_UNDEFINED && memory_object->type != MemoryObject::TYPE_BYTE) {
            cout << "[System::DisassemblyThread] stopping. cannot disassemble type " << memory_object->type << " at " << disassembly_address << endl;
            break;
        }

        u8 op = memory_object->bval;
        string inst = disassembler->GetInstruction(op);
        cout << "D: got opcode: $" << hex << uppercase << (int)memory_object->bval << " (" << inst << ")" << endl;

        int size = disassembler->GetInstructionSize(op);
        if(size == 0) {
            cout << "exiting because size is 0" << endl;
            break; // break on unimplemented opcodes (TODO remove me)
        }

        // convert the memory to code
        if(!memory_region->MarkMemoryAsCode(disassembly_address, size)) {
            cout << "exiting because MarkMemoryAsCode failed" << endl;
            break;
        }

        disassembly_address = disassembly_address + size;
    }

    // done
    disassembling = false;
    return 0;
}

BaseSystem::Information const* System::GetInformation()
{
    return System::GetInformationStatic();
}

BaseSystem::Information const* System::GetInformationStatic()
{
    static BaseSystem::Information information = {
        .abbreviation  = "NES",
        .full_name     = "Nintendo Entertainment System",
        .is_rom_valid  = std::bind(&System::IsROMValid, placeholders::_1, placeholders::_2),
        .create_system = std::bind(&System::CreateSystem)
    };

    return &information;
}

shared_ptr<BaseSystem> System::CreateSystem()
{
    System* nes_system = new System();
    return shared_ptr<BaseSystem>(nes_system);
}

}
