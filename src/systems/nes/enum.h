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

class BaseExpression;

namespace Systems::NES {

class Define;
class EnumElement;

using EnumReferenceable = Systems::Referenceable<GlobalMemoryLocation, Define>;

// An Enum is a collection of expressions under a named category. 
//
// Enums are allowed to have multiple names corresponding to the same value
// Enums can be used as operands in instructions, or as values in Defines
class Enum : public std::enable_shared_from_this<Enum>, public EnumReferenceable {
public:
    Enum(std::string const&);
    ~Enum();

    std::string const& GetName() const { return name; }

    std::shared_ptr<EnumElement> CreateElement(std::string const& name, std::shared_ptr<BaseExpression> const& expression);

    std::vector<std::shared_ptr<EnumElement>> const& GetElementsByValue(s64 value);
    std::shared_ptr<EnumElement> const& GetElement(std::string const& name);
    bool ChangeElementExpression(std::shared_ptr<EnumElement> const&, std::shared_ptr<BaseExpression> const&, std::string& errmsg);
    bool ChangeElementName(std::shared_ptr<EnumElement> const&, std::string const&, std::string& errmsg);
    void DeleteElement(std::shared_ptr<EnumElement> const&);
    void DeleteElements();

    template<typename F>
    void IterateElements(F f) {
        for(auto& pair : elements) {
            f(pair.second);
        }
    }

    template<typename F>
    void IterateElements(F f, s64 v) {
        for(auto& pair : elements) {
            if(pair.second->cached_value == v) {
                f(pair.second);
            }
        }
    }

    // signals
    typedef signal<std::function<void(std::shared_ptr<EnumElement> const&)>> enum_added_t;
    typedef signal<std::function<void(std::shared_ptr<EnumElement> const&, std::string const&, s64)>> enum_changed_t;

    std::shared_ptr<enum_added_t>   element_added;
    std::shared_ptr<enum_changed_t> element_changed;
    std::shared_ptr<enum_added_t>   element_deleted;

    bool Save(std::ostream&, std::string&) const;
    static std::shared_ptr<Enum> Load(std::istream&, std::string&);

private:
    void InsertElement(std::shared_ptr<EnumElement> const&);

    std::string name;

    std::unordered_map<std::string, std::shared_ptr<EnumElement>> elements;

    // map from value to element
    // this map is not saved in the project file, it is generated at runtime
    std::unordered_map<s64, std::vector<std::shared_ptr<EnumElement>>> value_map;
};

class EnumElement : public EnumReferenceable {
public:
    // simple name and complex expression
    s64                             cached_value;

    std::string                     const& GetName()       const { return name; }
    std::shared_ptr<BaseExpression> const& GetExpression() const { return expression; }

    inline std::string GetFormattedName(std::string const& sep) const { 
        if(auto e = parent_enum.lock()) {
            return e->GetName() + sep + name;
        }
        return GetName();
    }

    // reference to parent Enum
    std::weak_ptr<Enum>             parent_enum;

    bool Save(std::ostream&, std::string&) const;
    bool Load(std::istream&, std::string&);

private:
    std::string                     name;
    std::shared_ptr<BaseExpression> expression;

    friend class Enum;
};

}
