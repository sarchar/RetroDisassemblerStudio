// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <memory>

#include "main.h"
#include "signals.h"
#include "systems/system.h"
#include "windows/base_window.h"

class SNESMemory : public BaseWindow {
public:
    SNESMemory();
    virtual ~SNESMemory();

    virtual char const * const GetWindowClass() { return SNESMemory::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "SNESMemory"; }

    // signals

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;

public:
    static std::shared_ptr<SNESMemory> CreateWindow();
};
