// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <memory>
#include <string>

#include "systems/nes/memory.h"
#include "systems/nes/referenceable.h"

class BaseExpression;

namespace Systems {
    class BaseComment;
}

namespace Systems::NES {

// Defines can be referenced by memory and other defines
class Define : public std::enable_shared_from_this<Define>, 
               public Systems::Referenceable<GlobalMemoryLocation, Define, Systems::BaseComment> {
public:
    Define(std::string const&);
    ~Define();

    void SetReferences();
    void ClearReferences();

    void SetString(std::string const& s) { name = s; }
    bool SetExpression(std::string const&, std::string&);
    bool SetExpression(std::shared_ptr<BaseExpression> const&, std::string&);

    std::string                     const& GetName()       const { return name; }
    std::shared_ptr<BaseExpression> const& GetExpression() const { return expression; }

    s64 Evaluate();
    std::string GetExpressionString();

    bool Save(std::ostream&, std::string&);
    static std::shared_ptr<Define> Load(std::istream&, std::string&);

    bool operator==(Define const& other) {
        return name == other.name;
    }

    // signals

private:
    std::string                     name;
    std::shared_ptr<BaseExpression> expression;

    bool cached = false;
    s64 cached_value;
};

}
