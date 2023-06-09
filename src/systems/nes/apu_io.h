// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <functional>
#include <memory>

#include "signals.h"
#include "systems/nes/memory.h"

namespace Systems::NES {

#define NES_BUTTON_A      0
#define NES_BUTTON_B      1
#define NES_BUTTON_SELECT 2
#define NES_BUTTON_START  3
#define NES_BUTTON_UP     4
#define NES_BUTTON_DOWN   5
#define NES_BUTTON_LEFT   6
#define NES_BUTTON_RIGHT  7

class APU_IO_View;

// Since there's only a handful of non-apu registers in the I/O space at $4000,
// they're included here in the APU module
class APU_IO : public std::enable_shared_from_this<APU_IO> {
public:
    APU_IO();
    ~APU_IO();

    void SetJoy1Pressed(int button, bool pressed);
    void SetJoy2Pressed(int button, bool pressed);

    std::shared_ptr<MemoryView> CreateMemoryView();

    make_signal(oam_dma_callback, void(u8));

    bool Save(std::ostream&, std::string&) const;
    bool Load(std::istream&, std::string&);

    friend class APU_IO_View;

private:
    u8 joy1_state = 0;
    u8 joy1_state_latched;
    u8 joy2_state = 0;
    u8 joy2_state_latched;
};

class APU_IO_View : public MemoryView {
public:
    APU_IO_View(std::shared_ptr<APU_IO> const&);
    virtual ~APU_IO_View();

    u8 Peek(u16) override;
    u8 Read(u16) override;
    void Write(u16, u8) override;

    u8 ReadPPU(u16) override { return 0xFF; }
    void WritePPU(u16, u8) override {}

    bool Save(std::ostream& os, std::string& errmsg) const override;
    bool Load(std::istream& is, std::string& errmsg) override;

private:
    std::shared_ptr<APU_IO> apu_io;

    u8 joy1_probe = 0;
    u8 joy2_probe = 0;
};

};
