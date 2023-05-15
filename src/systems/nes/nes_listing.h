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

    ListingItem() {}
    virtual ~ListingItem() {}

    virtual void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&, u32, bool) = 0;

protected:
};

class ListingItemUnknown : public ListingItem {
public:
    ListingItemUnknown()
        : ListingItem()      
    { }
    virtual ~ListingItemUnknown() { }

    void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&, u32, bool) override;
};

class ListingItemBlankLine : public ListingItem {
public:
    ListingItemBlankLine()
        : ListingItem()      
    { }
    virtual ~ListingItemBlankLine() { }

    void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&, u32, bool) override;
};

class ListingItemData : public ListingItem {
public:
    ListingItemData(u32 _internal_offset)
        : ListingItem(), internal_offset(_internal_offset)
    { }
    virtual ~ListingItemData() { }

    void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&, u32, bool) override;

private:
    u32 internal_offset;
};

class ListingItemPreComment : public ListingItem {
public:
    ListingItemPreComment(int _line)
        : ListingItem(), line(_line)
    { }
    virtual ~ListingItemPreComment() { }

    void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&, u32, bool) override;
private:
    int line;
};

class ListingItemPostComment : public ListingItem {
public:
    ListingItemPostComment(int _line)
        : ListingItem(), line(_line)
    { }
    virtual ~ListingItemPostComment() { }

    void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&, u32, bool) override;
private:
    int line;
};

class ListingItemCode : public ListingItem {
public:
    ListingItemCode(int _line)
        : ListingItem(), line(_line)
    { }
    virtual ~ListingItemCode() { }

    void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&, u32, bool) override;

private:
    enum {
        EDIT_NONE,
        EDIT_OPERAND_EXPRESSION,
        EDIT_EOL_COMMENT
    } edit_mode = EDIT_NONE;

    int line;
    bool started_editing = false;
    std::string edit_buffer;
};

class ListingItemLabel : public ListingItem {
public:
    ListingItemLabel(std::shared_ptr<Label> const& _label, int _nth)
        : ListingItem(), label(_label), nth(_nth), editing(false)
    { }
    virtual ~ListingItemLabel() { }

    void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&, u32, bool) override;

private:
    std::shared_ptr<Label> const& label;
    int nth;
    std::string edit_buffer;
    bool editing;
    bool started_editing;
};

}

