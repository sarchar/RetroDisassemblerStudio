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
    GetMemoryRegion(vectors)->MarkMemoryAsWords(vectors, 6);

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

shared_ptr<Label> System::CreateLabel(GlobalMemoryLocation const& where, string const& label_str, bool was_user_created)
{
    // lookup the label to see 
    auto other = label_database[label_str];
    if(other) {
        cout << "[System::CreateLabel] label '" << label_str << "' already exists and points to " << where << endl;
        return nullptr;
    }

    auto label = make_shared<Label>(where, label_str);
    label_database[label_str] = label;

    auto memory_region = GetMemoryRegion(where);
    memory_region->ApplyLabel(label);

    // notify the system of new labels
    label_created->emit(label, was_user_created);

    return label;
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
        GlobalMemoryLocation loc = locations.front();
        locations.pop_front();

        bool disassembling_inner = true;
        while(disassembling_inner) {
            auto memory_region = GetMemoryRegion(loc);
            auto memory_object = memory_region->GetMemoryObject(loc);

            // bail on this disassemble if we already know the location is code
            if(memory_object->type == MemoryObject::TYPE_CODE) {
                //cout << "[NES::System::DisassemblyThread] address " << loc << " is already code" << endl;
                break;
            }

            // give up if we can't even convert this data to code. the user must clear the data type first
            if(memory_object->type != MemoryObject::TYPE_UNDEFINED && memory_object->type != MemoryObject::TYPE_BYTE) {
                cout << "[NES::System::DisassemblyThread] cannot disassemble type " << memory_object->type << " at " << loc << endl;
                break;
            }

            u8 op = memory_object->bval;
            string inst = disassembler->GetInstruction(op);

            int size = disassembler->GetInstructionSize(op);

            // stop disassembling on unknown opcodes
            if(size == 0) {
                cout << "[NES::System::DisassemblyThread] stopping because invalid opcode $" << hex << uppercase << (int)memory_object->bval << " (" << inst << ") at "  << loc << endl;
                break; // break on unimplemented opcodes (TODO remove me)
            }

            // convert the memory to code
            if(!memory_region->MarkMemoryAsCode(loc, size)) {
                assert(false); // this shouldn't happen
                break;
            }

            // next PC
            loc = loc + size;

            // certain instructions must stop disassembly and others cause branches
            switch(op) {
            case 0x4C: // JMP absolute. the only difference with JSR is that we finish this line of disassembly
                disassembling_inner = false;
                // fall through
            case 0x20: // JSR absolute
            {
                u16 target = (u16)memory_object->code.operands[0] | ((u16)memory_object->code.operands[1] << 8);
                if(target >= memory_region->GetBaseAddress() && target < memory_region->GetEndAddress()) {
                    GlobalMemoryLocation newloc(loc);
                    newloc.address = target;
                    locations.push_back(newloc);
                    //cout << "[NES::System::DisassemblyThread] continuing disassembling at " << newloc << endl;

                    // create a label at that address if there isn't one yet
                    auto destination_object = memory_region->GetMemoryObject(newloc);

                    stringstream ss;
                    ss << "L_" << hex << setw(2) << setfill('0') << uppercase << newloc.prg_rom_bank << setw(4) << newloc.address;
                    string label_str = ss.str();
                    if(auto label = CreateLabel(newloc, label_str)) {
                        auto expr         = make_shared<Expression>();
                        auto name         = expr->GetNodeCreator()->CreateName(label->GetString());
                        expr->Set(name);

                        // set the expression for memory object at current_selection. it'll show up immediately
                        memory_object->operand_expression = expr;
                    }
                }
                break;
            }

            case 0x60: // RTS
            case 0x6C: // JMP indirect
                disassembling_inner = false;
                break;
            }
        }
    }

    // leave the dialog up for at least a moment
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    disassembling = false;
    disassembly_stopped->emit(disassembly_address);
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
