#pragma once

#include "systems/nes/nes_defs.h"
#include "systems/nes/nes_memory.h"

namespace NES {

class System;
class ProgramRomBank;

//! // Content blocks can be data, code, or graphics
//! // Data can be structured, arrays, etc. So we will need types, arrays, etc
//! // Will need built-in types like byte, word, function pointer, etc.
//! struct ContentBlock {
//!     CONTENT_BLOCK_TYPE type;    // CODE statement, DATA block, CHR, possibly other things
//!     u16                offset;  // Offset within the rom bank that this content block starts at
//!     u16                num_listing_items; // number of listing items this ContentBlock produces in the Listing window
//! 
//!     union {
//!         struct {
//!             // code contentblock is always one instruction long
//!         } code;
//! 
//!         struct {
//!             CONTENT_BLOCK_DATA_TYPE type; // byte, word, pointer, user defined
//!             u16   count;   // Total number of elements
//!             u8    elements_per_line; // When displaying the contents of this data, how many elements per line we should display
//!             void* ptr;     // TODO will need a structure to hold various content blocks
//!         } data;
//! 
//!         struct {
//!         } chr;
//!     };
//! 
//!     u32 GetDataTypeSize() const {
//!         switch(data.type) {
//!         case CONTENT_BLOCK_DATA_TYPE_UBYTE:
//!             return 1;
//!         case CONTENT_BLOCK_DATA_TYPE_UWORD:
//!             return 2;
//!         default:
//!             assert(false);
//!             return 0;
//!         }
//!     }
//! 
//!     // return size in bytes
//!     u32 GetSize() const {
//!         switch(type) {
//!         case CONTENT_BLOCK_TYPE_DATA:
//!             return data.count * GetDataTypeSize();
//! 
//!         default:
//!             assert(false); // TODO
//!             return 0;
//!         }
//!     }
//! 
//!     std::string FormatInstructionField();
//!     std::string FormatDataElement(u16 n);
//! 
//! };

// A single ListingItem translates to a single row in the Listing window. A listing item can be
// all sorts of row types: comments, labels, actual code, data, etc.
class ListingItem {
public:
    ListingItem()
    { 
    }
    virtual ~ListingItem() { }

    //TYPE GetType() const { return item_type; }
    virtual void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&) = 0;

protected:
};

class ListingItemUnknown : public ListingItem {
public:
    ListingItemUnknown()
        : ListingItem()      
    { }
    virtual ~ListingItemUnknown() { }

    void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&) override;
};

class ListingItemData : public ListingItem {
public:
    ListingItemData(std::weak_ptr<MemoryRegion> _memory_region, u32 _internal_offset)
        : ListingItem(), memory_region(_memory_region), internal_offset(_internal_offset)
    { }
    virtual ~ListingItemData() { }

    void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&) override;

private:
    std::weak_ptr<MemoryRegion> memory_region;
    u32 internal_offset;
};

class ListingItemLabel : public ListingItem {
public:
    ListingItemLabel(std::string const& _name)
        : ListingItem(), label_name(_name) 
    { }
    virtual ~ListingItemLabel() { }

    void RenderContent(std::shared_ptr<System>&, GlobalMemoryLocation const&) override;

private:
    std::string label_name;
};

}

