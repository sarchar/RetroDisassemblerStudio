#pragma once

#include <memory>

namespace NES {

class MemoryView;
class PPUView;

class PPU : public std::enable_shared_from_this<PPU> {
public:
    PPU();
    ~PPU();

    void Reset();
    void Step();

    std::shared_ptr<MemoryView> CreateMemoryView();
private:

    friend class PPUView;
};

}
