#pragma once

#include <memory>
#include <string>

#include "util.h"
#include "systems/nes/defs.h"

namespace Systems::NES {

class Disassembler : public std::enable_shared_from_this<Disassembler> {
public:
    Disassembler();
    ~Disassembler();

    std::string GetInstruction(u8);
    char const* GetInstructionC(u8);
    int         GetInstructionSize(u8);

    ADDRESSING_MODE GetAddressingMode(u8);

    std::string FormatOperand(u8, u8 const*);
private:
};

}
