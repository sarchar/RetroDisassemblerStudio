// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <memory>
#include <stack>
#include <variant>

#include "signals.h"
#include "windows/basewindow.h"

#include "systems/nes/memory.h"
#include "systems/nes/referenceable.h"

namespace Systems {
    class BaseComment;
}

namespace Systems::NES {

class Define;
class EnumElement;
class Label;
class System;

}

namespace Windows::NES {

class References : public BaseWindow {
public:
    using BaseComment          = Systems::BaseComment;
    using Define               = Systems::NES::Define;
    using EnumElement          = Systems::NES::EnumElement;
    using GlobalMemoryLocation = Systems::NES::GlobalMemoryLocation;
    using Label                = Systems::NES::Label;
    using System               = Systems::NES::System;

    typedef std::variant<
        std::shared_ptr<Define>,
        std::shared_ptr<Label>,
        std::shared_ptr<EnumElement>> reference_type;

    References();
    References(reference_type const&);
    virtual ~References();

    virtual char const * const GetWindowClass() { return References::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "Windows::NES::References"; }
    static std::shared_ptr<References> CreateWindow();
    static std::shared_ptr<References> CreateWindow(reference_type const&);

    // signals

protected:
    void Update(double deltaTime) override;
    void Render() override;

private:
    std::weak_ptr<System>            current_system;
    reference_type                   reference_to;

    int selected_row;
    bool need_resort     = true;
    bool need_repopulate = true;

    signal_connection changed_connection;
    signal_connection label_deleted_connection;

    typedef std::variant<
        std::shared_ptr<GlobalMemoryLocation>,
        std::shared_ptr<Define>,
        std::shared_ptr<EnumElement>,
        std::shared_ptr<BaseComment>
    > location_type;
    std::vector<location_type> locations;

    void PopulateLocations();

    template <class T>
    void PopulateLocations(std::shared_ptr<T>& target)
    {
        target->IterateReverseReferences([this](int i, T::reverse_reference_t const& rref) {
			locations.push_back(variant_cast(rref));
        }); // call to IterateReverseReferences
    }
};

} //namespace Windows::NES

