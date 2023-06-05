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

namespace Systems::NES {

class Label : public std::enable_shared_from_this<Label> {
public:
    Label(GlobalMemoryLocation const&, std::string const&);
    ~Label();

    // signals
    typedef signal<std::function<void()>> reverse_references_changed_t;
    std::shared_ptr<reverse_references_changed_t> reverse_references_changed;

    typedef signal<std::function<void(std::shared_ptr<Label>, int)>> index_changed_t;
    std::shared_ptr<index_changed_t> index_changed;

    void SetString(std::string const& s) { label = s; }

    void                 SetIndex(int _index)             { index = _index; index_changed->emit(shared_from_this(), index); }
    int                  const& GetIndex()          const { return index; }
    GlobalMemoryLocation const& GetMemoryLocation() const { return memory_location; }
    std::string          const& GetString()         const { return label; }

    int GetNumReverseReferences() const { 
        return reverse_references.size(); 
    }

    void NoteReference(GlobalMemoryLocation const& where) {
        int size = reverse_references.size();
        reverse_references.insert(where);
        if(reverse_references.size() != size) reverse_references_changed->emit();
    }

    int RemoveReference(GlobalMemoryLocation const& where) {
        int size = reverse_references.size();
        auto ret = reverse_references.erase(where);
        if(reverse_references.size() != size) reverse_references_changed->emit();
        return ret;
    }

    template<typename F>
    void IterateReverseReferences(F func) {
        int i = 0;
        for(auto& where : reverse_references) {
            func(i++, where);
        }
    }

    bool Save(std::ostream&, std::string&);
    static std::shared_ptr<Label> Load(std::istream&, std::string&);
private:
    int                  index;  // not serialized in save, calculated in at runtime
    GlobalMemoryLocation memory_location;
    std::string          label;

    std::unordered_set<GlobalMemoryLocation> reverse_references;
};

}
