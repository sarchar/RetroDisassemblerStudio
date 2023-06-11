// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 

// TODO this file is in src/systems/nes/ because it uses GlobalMemoryLocation, 
// which isn't generic yet. At some point this should probably also be generic
// However, it's not in the NES namespace

#pragma once

#include <memory>
#include <unordered_set>
#include <variant>

#include "signals.h"
#include "util.h"

namespace Systems {

// Each reference_type must implement operator==()
template<typename... reference_types>
class Referenceable {
public:
    typedef std::variant<std::shared_ptr<reference_types>...> reverse_reference_t;

    Referenceable() {
        reverse_references_changed = std::make_shared<reverse_references_changed_t>();
    }

    ~Referenceable() {
    }

    int GetNumReverseReferences() const { 
        return reverse_references.size(); 
    }

    template<class T>
    void NoteReference(T const& t) {
        auto it = FindReverseReference(t);
        if(it == reverse_references.end()) {
            reverse_references.insert(t);
            reverse_references_changed->emit();
        }
    }

    template<class T>
    bool RemoveReference(T const& t) {
        auto it = FindReverseReference(t);
        if(it != reverse_references.end()) {
            reverse_references.erase(it);
            reverse_references_changed->emit();
            return true;
        }
        return false;
    }

    template<typename F>
    void IterateReverseReferences(F func) {
        int i = 0;
        for(auto& v : reverse_references) {
            func(i++, v);
        }
    }

    template<class T, typename F>
    void IterateReverseReferencesOf(F func) {
        int i = 0;
        for(auto& rref : reverse_references) {
            if(auto v = std::get_if<std::shared_ptr<T>>(&rref)) {
                func(i++, *v);
            }
        }
    }

    // signals
    typedef signal<std::function<void()>> reverse_references_changed_t;
    std::shared_ptr<reverse_references_changed_t> reverse_references_changed;

private:
    template<class T>
    std::unordered_set<reverse_reference_t>::iterator FindReverseReference(T const& t) {
        return std::find_if(reverse_references.begin(), reverse_references.end(), 
            [&t](reverse_reference_t const& v)->bool {
                if(auto vt = std::get_if<T>(&v)) {
                    return (*vt).get()->operator==(*t.get());
                }
                return false;
            }
        );
    }

    std::unordered_set<reverse_reference_t> reverse_references;
};

};
