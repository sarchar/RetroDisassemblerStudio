#pragma once

#include <memory>
#include <string>

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

    bool Save(std::ostream&, std::string&);
    static std::shared_ptr<Label> Load(std::istream&, std::string&);
private:
    int                  index;  // not serialized in save, calculated in at runtime
    GlobalMemoryLocation memory_location;
    std::string          label;
};

}
