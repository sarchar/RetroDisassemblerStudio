// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <memory>
#include <string>
#include <unordered_set>

#include "signals.h"

#include "systems/nes/memory.h"
#include "systems/nes/referenceable.h"

namespace Systems {
    class BaseComment;
}

namespace Systems::NES {

class Label : public std::enable_shared_from_this<Label>, 
              public Systems::Referenceable<MemoryObjectOperandReference, Systems::BaseComment> {
public:
    Label(GlobalMemoryLocation const&, std::string const&);
    ~Label();

    // signals
    make_signal(index_changed, void(std::shared_ptr<Label>, int));

    void SetString(std::string const& s) { label = s; }

    void                 SetIndex(int _index)             { index = _index; index_changed->emit(shared_from_this(), index); }
    int                  const& GetIndex()          const { return index; }
    GlobalMemoryLocation const& GetMemoryLocation() const { return memory_location; }
    std::string          const& GetString()         const { return label; }

    bool Save(std::ostream&, std::string&);
    static std::shared_ptr<Label> Load(std::istream&, std::string&);

    bool operator==(Label const& other) {
        return label == other.label;
    }

private:
    int                  index;  // not serialized in save, calculated in at runtime
    GlobalMemoryLocation memory_location;
    std::string          label;
};

}
