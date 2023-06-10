// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <deque>
#include <variant>

#include "systems/nes/defs.h"
#include "systems/nes/memory.h"

namespace Systems::NES {
    class Define;
    class EnumElement;
    class Label;
    class GlobalMemoryLocation;
    class MemoryObject;
    class System;
    class ProgramRomBank;
}

namespace Windows::NES {

class SystemInstance;

// A single ListingItem translates to a single row in the Listing window. A listing item can be
// all sorts of row types: comments, labels, actual code, data, etc.
class ListingItem {
public:
    using Define = Systems::NES::Define;
    using EnumElement = Systems::NES::EnumElement;
    using Label  = Systems::NES::Label;
    using GlobalMemoryLocation = Systems::NES::GlobalMemoryLocation;
    using MemoryObject = Systems::NES::MemoryObject;
    using ProgramRomBank = Systems::NES::ProgramRomBank;
    using System = Systems::NES::System;

    typedef std::function<void()> postponed_change_t;
    typedef std::deque<postponed_change_t> postponed_changes;

    static unsigned long common_inner_table_flags;

    ListingItem() {}
    virtual ~ListingItem() {}

    virtual void Render(std::shared_ptr<Windows::NES::SystemInstance> const&, std::shared_ptr<System>&, GlobalMemoryLocation const&, 
            u32, bool, bool, bool, postponed_changes&) = 0;
    virtual bool IsEditing() const = 0;

protected:
};

class ListingItemUnknown : public ListingItem {
public:
    ListingItemUnknown()
        : ListingItem()      
    { }
    virtual ~ListingItemUnknown() { }

    void Render(std::shared_ptr<Windows::NES::SystemInstance> const&, std::shared_ptr<System>&, GlobalMemoryLocation const&, 
            u32, bool, bool, bool, postponed_changes&) override;
    bool IsEditing() const override { return false; }
};

class ListingItemBlankLine : public ListingItem {
public:
    ListingItemBlankLine()
        : ListingItem()      
    { }
    virtual ~ListingItemBlankLine() { }

    void Render(std::shared_ptr<Windows::NES::SystemInstance> const&, std::shared_ptr<System>&, GlobalMemoryLocation const&, 
            u32, bool, bool, bool, postponed_changes&) override;
    bool IsEditing() const override { return false; }
};

class ListingItemPrePostComment : public ListingItem {
public:
    ListingItemPrePostComment(int _line, bool _is_post)
        : ListingItem(), line(_line), is_post(_is_post)
    { }
    virtual ~ListingItemPrePostComment() { }

    void Render(std::shared_ptr<Windows::NES::SystemInstance> const&, std::shared_ptr<System>&, GlobalMemoryLocation const&, 
            u32, bool, bool, bool, postponed_changes&) override;
    bool IsEditing() const override;
private:
    int line;
    bool is_post;
};

class ListingItemPrimary : public ListingItem {
public:
    ListingItemPrimary(int _line)
        : ListingItem(), line(_line)
    { }
    virtual ~ListingItemPrimary() { }

    void Render(std::shared_ptr<Windows::NES::SystemInstance> const&, std::shared_ptr<System>&, GlobalMemoryLocation const&, 
            u32, bool, bool, bool, postponed_changes&) override;

    void EditOperandExpression(std::shared_ptr<System>&, GlobalMemoryLocation const&);
    bool ParseOperandExpression(std::shared_ptr<System>&, GlobalMemoryLocation const&);
    void ResetOperandExpression(std::shared_ptr<System>&, GlobalMemoryLocation const&);
    void NextLabelReference(std::shared_ptr<System>&, GlobalMemoryLocation const&);
    bool IsEditing() const override;

private:
    enum {
        // reverse order of the columns so that they hide when editing
        EDIT_NONE,
        EDIT_EOL_COMMENT,
        EDIT_OPERAND_EXPRESSION
    } edit_mode = EDIT_NONE;

    int line;
    bool started_editing = false;
    std::string edit_buffer;

    bool do_parse_operand_expression = false;
    bool wait_dialog = false;
    std::string parse_errmsg;

    typedef std::variant<
        std::shared_ptr<Define>,
        std::shared_ptr<Label>,
        std::shared_ptr<EnumElement>
    > suggestion_type;
    std::vector<suggestion_type> suggestions;

    int suggestion_start;
    bool deselect_input = false;

    void RecalculateSuggestions(std::shared_ptr<System>&);
    int  EditOperandExpressionTextCallback(void*);
    void RenderEditOperandExpression(std::shared_ptr<System>&);
};

class ListingItemLabel : public ListingItem {
public:
    ListingItemLabel(std::shared_ptr<Label> const& _label, int _nth)
        : ListingItem(), label(_label), nth(_nth), editing(false)
    { }
    virtual ~ListingItemLabel() { }

    void Render(std::shared_ptr<Windows::NES::SystemInstance> const&, std::shared_ptr<System>&, GlobalMemoryLocation const&, 
            u32, bool, bool, bool, postponed_changes&) override;
    bool IsEditing() const override;

private:
    std::shared_ptr<Label> const label;
    int nth;
    std::string edit_buffer;
    bool editing;
    bool started_editing;
};

} // namespace Windows::NES

