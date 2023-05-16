#include <cassert>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "main.h"
#include "util.h"

#include "systems/nes/nes_disasm.h"
#include "systems/nes/nes_expressions.h"
#include "systems/nes/nes_label.h"
#include "systems/nes/nes_listing.h"
#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_system.h"

using namespace std;

namespace NES {

unsigned long ListingItem::common_inner_table_flags = ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Resizable;

void ListingItemUnknown::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool selected)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(ImGui::BeginTable("listing_item_unknown", 1, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("??");
        ImGui::EndTable();
    }
}

void ListingItemBlankLine::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool selected)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(ImGui::BeginTable("listing_item_blank", 1, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::Text("");

        ImGui::EndTable();
    }
}

void ListingItemData::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool selected)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(ImGui::BeginTable("listing_item_code", 6, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Raw", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Mnemonic", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Operand", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("EOLComment", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableNextRow();
    
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, (ImU32)ImColor(200, 200, 200));
        ImGui::Text("$%02X:0x%04X", where.prg_rom_bank, where.address);

        ImGui::TableNextColumn(); // spacing

    
        if(auto memory_object = system->GetMemoryObject(where)) {
            ImGui::TableNextColumn(); // raw display
            {
                int objsize = memory_object->GetSize();
                stringstream ss;
                ss << hex << setfill('0') << uppercase;
                for(int i = 0; i < objsize; i++) {
                    ss << setw(2) << (int)((u8*)&memory_object->bval)[i];
                    if(i != (objsize - 1)) ss << " ";
                }

                ImGui::Text("%s", ss.str().c_str());
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", memory_object->FormatInstructionField().c_str());

            // The internal_offset value will be used to index into the middle of data arrays, so that
            // multiple data listing items can show something like:
            // 
            // .DB $01, $02, $03,
            //     $04, $05, $06
            //     $07
            //
            ImGui::TableNextColumn();
            ImGui::Text("%s", memory_object->FormatOperandField(internal_offset).c_str());

            ImGui::TableNextColumn();
            if(auto eolc = memory_object->comments.eol) {
                ImGui::Text("; %s", eolc->c_str()); // TODO multiline
            }
        }

        ImGui::EndTable();
    }
}

void ListingItemPreComment::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool selected)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(ImGui::BeginTable("listing_item_comment2", 2, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthFixed, 4.0f);
        ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        
        ImGui::TableNextColumn();
        ImGui::Text("        ");
    
        ImGui::TableNextColumn();
        string precomment;
        system->GetComment(where, MemoryObject::COMMENT_TYPE_PRE, precomment); // TODO multiline
        if(selected) {
            ImGui::InputTextMultiline("", &precomment, ImVec2(0, 0), 0);
        } else {
            ImGui::Text("; %s", precomment.c_str());
        }

        ImGui::EndTable();
    }
}

void ListingItemPostComment::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool selected)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(ImGui::BeginTable("listing_item_comment2", 2, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthFixed, 4.0f);
        ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthFixed);

        ImGui::TableNextRow();
        
        ImGui::TableNextColumn();
        ImGui::Text("        ");

        ImGui::TableNextColumn();
        string postcomment;
        system->GetComment(where, MemoryObject::COMMENT_TYPE_POST, postcomment); // TODO multiline
        ImGui::Text("; %s", postcomment.c_str());

        ImGui::EndTable();
    }
}



void ListingItemCode::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool selected)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    auto memory_object = system->GetMemoryObject(where);
    if(!memory_object) return;
    auto disassembler = system->GetDisassembler();

    if(selected && edit_mode == EDIT_NONE) {
        if(ImGui::IsKeyPressed(ImGuiKey_Semicolon)) { // edit the EOL comment
            system->GetComment(where, MemoryObject::COMMENT_TYPE_EOL, edit_buffer);
            edit_mode = EDIT_EOL_COMMENT;
            started_editing = true;
        } else if(ImGui::IsKeyPressed(ImGuiKey_Enter)) { // edit the operand expression
            edit_buffer = memory_object->FormatOperandField(0, disassembler);
            edit_mode = EDIT_OPERAND_EXPRESSION;
            started_editing = true;
        }
    } 

    if(!selected || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        edit_mode = EDIT_NONE;
    }


    if(ImGui::BeginTable("listing_item_code", 6, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Raw", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Mnemonic", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Operand", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("EOLComment", ImGuiTableColumnFlags_WidthFixed);

        ImGui::TableNextRow();
    
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, (ImU32)ImColor(200, 200, 200));
        ImGui::Text("$%02X:0x%04X", where.prg_rom_bank, where.address);

        ImGui::TableNextColumn(); // spacing

        ImGui::TableNextColumn();
        {
            int objsize = memory_object->GetSize();
            stringstream ss;
            ss << hex << setfill('0') << uppercase;
            for(int i = 0; i < objsize; i++) {
                ss << setw(2) << (int)((u8*)&memory_object->bval)[i];
                if(i != (objsize - 1)) ss << " ";
            }

            ImGui::Text("%s", ss.str().c_str());
        }

        ImGui::TableNextColumn();
        ImGui::Text("%s", memory_object->FormatInstructionField(disassembler).c_str());

        ImGui::TableNextColumn();
        if(edit_mode == EDIT_OPERAND_EXPRESSION) {
            if(started_editing) {
                ImGui::SetKeyboardFocusHere();
                started_editing = false;
            }
            if(ImGui::InputText("", &edit_buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
                parse_operand_expression = true;
            }
            if(parse_operand_expression && ParseOperandExpression(system, where)) {
                edit_mode = EDIT_NONE;
            }
        } else {
            string operand = memory_object->FormatOperandField(0, disassembler);
            ImGui::Text("%s", operand.c_str());
            if(ImGui::IsMouseDoubleClicked(0)) { // edit on double click
                edit_buffer = operand;
                edit_mode = EDIT_OPERAND_EXPRESSION;
                started_editing = true;
            }
        }

        ImGui::TableNextColumn();
        if(edit_mode == EDIT_EOL_COMMENT) {
            if(started_editing) {
                ImGui::SetKeyboardFocusHere();
                started_editing = false;
            }
            if(ImGui::InputText("", &edit_buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
                system->SetComment(where, MemoryObject::COMMENT_TYPE_EOL, edit_buffer);
                edit_mode = EDIT_NONE;
            }
        } else {
            string eol_comment;
            system->GetComment(where, MemoryObject::COMMENT_TYPE_EOL, eol_comment); // TODO multiline
            if(eol_comment.size()) {
                ImGui::Text("; %s", eol_comment.c_str());
                if(ImGui::IsMouseDoubleClicked(0)) { // edit on double click
                    edit_buffer = eol_comment;
                    edit_mode = EDIT_EOL_COMMENT;
                    started_editing = true;
                }
            }
        }

        ImGui::EndTable();
    }
}

// TODO
// parsing the expression should determine if Names can be converted to Labels or Defines at parse time
// any Name that can't be converted can be left as a Name operand
// and then that doesn't determine if a parsed expression is a legal NES expression
// we should have a validate instruction that makes sure that the opcode matches the operand expression
// and that all names are valid. Evaluate() should always be possible after accounting for addressing modes
bool ListingItemCode::ParseOperandExpression(shared_ptr<System>& system, GlobalMemoryLocation const& where)
{
    if(!wait_dialog) {
        int errloc;

        auto expr = make_shared<Expression>();
        if(expr->Set(edit_buffer, parse_errmsg, errloc)) {
            // successfully parsed the expression, so set it
            system->SetOperandExpression(where, expr);
            parse_operand_expression = false;
            return true;
        } else {
            wait_dialog = true;
            stringstream ss;
            ss << "The input expression isn't valid: " << parse_errmsg << " at position " << (errloc + 1);
            parse_errmsg = ss.str();
        }
    }

    if(wait_dialog) {
        if(MyApp::Instance()->OKPopup("Operand parse error", parse_errmsg)) {
            wait_dialog = false;
            parse_operand_expression = false;
        }
    }

    return false;
}

void ListingItemLabel::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool selected)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }
    
    if(selected && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        editing = true;
        started_editing = true;
    }

    if(editing) {
        if(!selected || ImGui::IsKeyPressed(ImGuiKey_Escape)) { // must stop editing, discard
            editing = false;
        }
    }

    if(ImGui::BeginTable("listing_item_label", 2, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableNextRow();
    
        ImGui::TableNextColumn();
        ImGui::Text("        ");

        ImGui::TableNextColumn();

        if(editing) {
            if(started_editing) {
                ImGui::SetKeyboardFocusHere();
                edit_buffer = label->GetString();
                started_editing = false;
            }

            if(ImGui::InputText("", &edit_buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
                if(edit_buffer.size() > 0) {
                    system->EditLabel(where, edit_buffer, nth, true);
                }
                editing = false;
            }
        } else {
            ImGui::Text("%s:", label->GetString().c_str());

            // start editing the label if double click happened
            if(selected && ImGui::IsMouseDoubleClicked(0)) {
                editing = true;
                started_editing = true;
            }
        }
    
        ImGui::EndTable();
    }
}


}
