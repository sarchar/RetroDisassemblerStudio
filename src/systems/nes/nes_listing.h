#pragma once

#include "systems/nes/nes_defs.h"
#include "systems/nes/nes_memory.h"

namespace NES {

class Label;
class System;
class ProgramRomBank;

// A single ListingItem translates to a single row in the Listing window. A listing item can be
// all sorts of row types: comments, labels, actual code, data, etc.
class ListingItem {
public:
    static unsigned long common_inner_table_flags;

    ListingItem()
    { 
    }
    virtual ~ListingItem() { }

    virtual void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&, u32) = 0;

protected:
};

class ListingItemUnknown : public ListingItem {
public:
    ListingItemUnknown()
        : ListingItem()      
    { }
    virtual ~ListingItemUnknown() { }

    void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&, u32) override;
};

class ListingItemData : public ListingItem {
public:
    ListingItemData(std::weak_ptr<MemoryRegion> _memory_region, u32 _internal_offset)
        : ListingItem(), memory_region(_memory_region), internal_offset(_internal_offset)
    { }
    virtual ~ListingItemData() { }

    void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&, u32) override;

private:
    std::weak_ptr<MemoryRegion> memory_region;
    u32 internal_offset;
};

class ListingItemCode : public ListingItem {
public:
    ListingItemCode(std::weak_ptr<MemoryRegion> _memory_region)
        : ListingItem(), memory_region(_memory_region)
    { }
    virtual ~ListingItemCode() { }

    void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&, u32) override;

private:
    std::weak_ptr<MemoryRegion> memory_region;
};

class ListingItemLabel : public ListingItem {
public:
    ListingItemLabel(std::shared_ptr<Label> const& _label)
        : ListingItem(), label(_label) 
    { }
    virtual ~ListingItemLabel() { }

    void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&, u32) override;

private:
    std::shared_ptr<Label> const& label;
};

}

