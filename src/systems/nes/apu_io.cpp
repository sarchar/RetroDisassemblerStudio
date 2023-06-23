// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#include <memory>

#include "util.h"

#include "systems/nes/apu_io.h"
#include "systems/nes/system.h"

using namespace std;

namespace Systems::NES {

APU_IO::APU_IO()
{
}

APU_IO::~APU_IO()
{
}


void APU_IO::SetJoy1Pressed(int button, bool pressed)
{
    assert(button >= 0 && button <= NES_BUTTON_RIGHT);
    if(pressed) joy1_state |= (1 << button);
    else        joy1_state &= ~(1 << button);
}

void APU_IO::SetJoy2Pressed(int button, bool pressed)
{
    assert(button >= 0 && button <= NES_BUTTON_RIGHT);
    if(pressed) joy2_state |= (1 << button);
    else        joy2_state &= ~(1 << button);
}

std::shared_ptr<MemoryView> APU_IO::CreateMemoryView()
{
    return make_shared<APU_IO_View>(shared_from_this());
}

bool APU_IO::Save(ostream& os, string& errmsg) const
{
    WriteVarInt(os, 0);

    WriteVarInt(os, joy1_state);
    WriteVarInt(os, joy1_state_latched);

    WriteVarInt(os, joy2_state);
    WriteVarInt(os, joy2_state_latched);

    errmsg = "Error saving APU_IO";
    return os.good();
}

bool APU_IO::Load(istream& is, string& errmsg)
{
    auto r = ReadVarInt<int>(is);
    assert(r == 0);

    joy1_state = ReadVarInt<u8>(is);
    joy1_state_latched = ReadVarInt<u8>(is);

    joy2_state = ReadVarInt<u8>(is);
    joy2_state_latched = ReadVarInt<u8>(is);

    errmsg = "Error loading APU_IO";
    return is.good();
}


APU_IO_View::APU_IO_View(std::shared_ptr<APU_IO> const& _apu_io)
    : apu_io(_apu_io)
{
}

APU_IO_View::~APU_IO_View()
{
}


u8 APU_IO_View::Peek(u16 address)
{
    u8 ret = 0x00;

    switch(address) {
    case 0x16:
        ret = apu_io->joy1_state_latched & 0x01;
        break;

    case 0x17:
        ret = apu_io->joy2_state_latched & 0x01;
        break;

    default:
        //cout << "[APU_IO_View::Read] unhandled peek from $" << hex << address << endl;
        break;
    }

    return ret;
}

u8 APU_IO_View::Read(u16 address)
{
    u8 ret = 0x00;

    switch(address) {
    case 0x16:
        ret = apu_io->joy1_state_latched & 0x01;
        apu_io->joy1_state_latched >>= 1;
        break;

    case 0x17:
        ret = apu_io->joy2_state_latched & 0x01;
        apu_io->joy2_state_latched >>= 1;
        break;

    default:
        //cout << "[APU_IO_View::Read] unhandled read from $" << hex << address << endl;
        break;
    }

    return ret;
}

void APU_IO_View::Write(u16 address, u8 value)
{
    switch(address) {
    case 0x14: // OAMDMA
        apu_io->oam_dma_callback->emit(value);
        break;

    case 0x16:
        if(value & 1) joy1_probe = 1;
        else if(!(value & 1) && joy1_probe) {
            joy1_probe = 0;
            apu_io->joy1_state_latched = apu_io->joy1_state;
        }
        break;

    case 0x17:
        if(value & 1) joy2_probe = 1;
        else if(!(value & 1) && joy2_probe) {
            joy2_probe = 0;
            apu_io->joy2_state_latched = apu_io->joy2_state;
        }
        break;

    default:
        //cout << "[APU_IO_View::Write] unhandled write to $" << hex << address << " value $" << (int)value << endl;
        break;
    }
}

bool APU_IO_View::Save(ostream& os, string& errmsg) const 
{
    WriteVarInt(os, 0);
    WriteVarInt(os, joy1_probe);
    WriteVarInt(os, joy2_probe);
    errmsg = "Error saving APU_IO_View";
    return os.good();
}

bool APU_IO_View::Load(istream& is, string& errmsg) 
{
    auto r = ReadVarInt<int>(is);
    assert(r == 0);
    joy1_probe = ReadVarInt<u8>(is);
    joy2_probe = ReadVarInt<u8>(is);
    errmsg = "Error loading APU_IO_View";
    return is.good();
}

}


