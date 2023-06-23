// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#include <iostream>
#include <memory>
#include <string>

#include "systems/nes/enum.h"
#include "systems/nes/expressions.h"

#include "windows/nes/project.h"

using namespace std;

namespace Systems::NES {

Enum::Enum(string const& _name)
    : name(_name)
{
}

Enum::~Enum()
{
}

void Enum::InsertElement(std::shared_ptr<EnumElement> const& ee)
{
    ee->parent_enum = shared_from_this();
    elements[ee->name]  = ee;
    value_map[ee->cached_value].push_back(ee);
}

shared_ptr<EnumElement> Enum::CreateElement(string const& name, shared_ptr<BaseExpression> const& expression)
{
    // fail if this enum already `name`
    if(elements.contains(name)) return nullptr;

    // attempt to evaluate the expression
    s64 result;
    string errmsg;
    if(!expression->Evaluate(&result, errmsg)) return nullptr;

    auto ee = make_shared<EnumElement>();
    ee->name         = name;
    ee->expression   = expression;
    ee->cached_value = result;
    InsertElement(ee);

    element_added->emit(ee);

    return ee;
}

vector<shared_ptr<EnumElement>> const& Enum::GetElementsByValue(s64 value)
{
    static vector<shared_ptr<EnumElement>> empty_list;
    if(!value_map.contains(value)) return empty_list;
    return value_map[value];
}

shared_ptr<EnumElement> const& Enum::GetElement(string const& name)
{
    static shared_ptr<EnumElement> null_element;
    if(elements.contains(name)) return elements[name];
    return null_element;
}

bool Enum::ChangeElementExpression(shared_ptr<EnumElement> const& ee, shared_ptr<BaseExpression> const& expression, string& errmsg)
{
    // first make sure expression is evaluateable
    s64 result;
    if(!expression->Evaluate(&result, errmsg)) return false;

    // if ee has things referring to it, we can't change the value
    if(ee->GetNumReverseReferences()) {
        if(result != ee->cached_value) {
            errmsg = "Cannot change enum value when it is being used in other expressions";
            return false;
        }
    }

    // update cached_value and expression now
    auto old_value = ee->cached_value;
    ee->cached_value = result;
    ee->expression = expression;

    // change this element to a different value_map if necessary
    if(old_value != ee->cached_value) {
        auto& list = value_map[old_value];
        auto it = find(list.begin(), list.end(), ee);
        assert(it != list.end());
        list.erase(it);

        // insert element with new value
        InsertElement(ee);
    }

    // emit changed signal
    element_changed->emit(ee, ee->name, old_value);
    return true;
}

bool Enum::ChangeElementName(shared_ptr<EnumElement> const& ee, string const& name, string& errmsg)
{
    // update name and emit
    string old_name = ee->name;
    ee->name = name;
    element_changed->emit(ee, old_name, ee->cached_value);
    return true;
}


void Enum::DeleteElement(shared_ptr<EnumElement> const& ee)
{
    if(!elements.contains(ee->name)) return;
    assert(value_map.contains(ee->cached_value));

    auto& list = value_map[ee->cached_value];
    auto it = find(list.begin(), list.end(), ee);
    assert(it != list.end());
    list.erase(it);

    elements.erase(ee->name);

    element_deleted->emit(ee);
}

void Enum::DeleteElements()
{
    auto mapcopy = elements;
    for(auto iter: mapcopy) DeleteElement(iter.second);
}

bool Enum::Save(ostream& os, string& errmsg) const
{
    WriteVarInt(os, 0); // reserved
    WriteVarInt(os, size);

    WriteString(os, name);
    WriteVarInt(os, elements.size());
    if(!os.good()) goto done;

    for(auto& pair : elements) {
        if(!pair.second->Save(os, errmsg)) return false;
    }

done:
    errmsg = "Error saving Enum";
    return os.good();
}

shared_ptr<Enum> Enum::Load(istream& is, string& errmsg)
{
    int size = 1;
    if(GetCurrentProject()->GetSaveFileVersion() >= FILE_VERSION_ENUMSIZE) {
        ReadVarInt<int>(is); // reserved
        size = ReadVarInt<int>(is);
    }

    string name;
    ReadString(is, name);

    int count = ReadVarInt<int>(is);
    if(!is.good()) {
        errmsg = "Error loading Enum";
        return nullptr;
    }

    auto e = make_shared<Enum>(name);
    e->size = size;
    for(int i = 0; i < count; i++) {
        auto ee = make_shared<EnumElement>();
        if(!ee->Load(is, errmsg)) return nullptr;
        e->InsertElement(ee);
    }

    return e;
}

bool EnumElement::Save(ostream& os, string& errmsg) const
{
    WriteString(os, name);
    WriteVarInt(os, cached_value);
    if(!os.good()) goto done;

    if(!expression->Save(os, errmsg)) return false;

done:
    errmsg = "Error saving EnumElement";
    return os.good();
}

bool EnumElement::Load(istream& is, string& errmsg)
{
    ReadString(is, name);
    cached_value = ReadVarInt<s64>(is);
    if(!is.good()) goto done;

    expression = make_shared<Systems::NES::Expression>();
    if(!expression->Load(is, errmsg)) return false;

done:
    errmsg = "Error loading EnumElement";
    return true;
}

}
