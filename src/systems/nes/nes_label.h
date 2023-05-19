#pragma once

#include <memory>
#include <string>
#include <unordered_set>

#include "systems/nes/nes_memory.h"

namespace NES {

class Label : public std::enable_shared_from_this<Label> {
public:
    Label(GlobalMemoryLocation const&, std::string const&);
    ~Label();

    void SetString(std::string const& s) { label = s; }

    void                 SetIndex(int _index)             { index = _index; }
    int                  const& GetIndex()          const { return index; }
    GlobalMemoryLocation const& GetMemoryLocation() const { return memory_location; }
    std::string          const& GetString()         const { return label; }

    int GetNumReverseReferences() const { 
        return reverse_references.size(); 
    }

    void NoteReference(GlobalMemoryLocation const& where) {
        reverse_references.insert(where);
    }

    int RemoveReference(GlobalMemoryLocation const& where) {
        return reverse_references.erase(where);
    }

    template<typename F>
    void IterateReverseReferences(F func) {
        for(auto& where : reverse_references) {
            func(where);
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
