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

#include "windows/nes/references.h"

#include "systems/nes/nes_disasm.h"
#include "systems/nes/nes_expressions.h"
#include "systems/nes/nes_label.h"
#include "systems/nes/nes_listing.h"
#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_system.h"

using namespace std;

namespace NES {

unsigned long ListingItem::common_inner_table_flags = ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Resizable;

void ListingItemUnknown::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool selected, bool hovered)
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

void ListingItemBlankLine::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool selected, bool hovered)
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

void ListingItemPrePostComment::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool selected, bool hovered)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(ImGui::BeginTable(is_post ? "listing_item_postcomment" : "listing_item_precomment", 2, table_flags)) {
        ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthFixed, 4.0f);
        ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        
        ImGui::TableNextColumn();
        ImGui::Text("        ");
    
        ImGui::TableNextColumn();
        string comment;
        system->GetComment(where, is_post ? MemoryObject::COMMENT_TYPE_POST : MemoryObject::COMMENT_TYPE_PRE, comment); // TODO multiline
        //!if(selected) {
        //!    ImGui::InputTextMultiline("", &comment, ImVec2(0, 0), 0);
        //!} else {
            ImGui::Text("; %s", comment.c_str());
        //!}

        ImGui::EndTable();
    }
}

bool ListingItemPrePostComment::IsEditing() const 
{
    return false;
}

void ListingItemPrimary::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool selected, bool hovered)
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
            EditOperandExpression(system, where);
        } else if(ImGui::IsKeyPressed(ImGuiKey_Backspace)) { // clear labels
            ResetOperandExpression(system, where);
        } else if(ImGui::IsKeyPressed(ImGuiKey_A)) { // next label
            NextLabelReference(system, where);
        }
    } 

    if(!selected || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        edit_mode = EDIT_NONE;
    }


    if(ImGui::BeginTable("listing_item_primary", 6, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Raw", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Mnemonic", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Operand", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("EOLComment", ImGuiTableColumnFlags_WidthStretch); // stretch comment to EOL

        ImGui::TableNextRow();
    
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, (ImU32)ImColor(200, 200, 200, (selected || hovered) ? 128 : 255));
        ImGui::Text("$%02X:0x%04X", where.prg_rom_bank, where.address);

        ImGui::TableNextColumn(); // spacing

        ImGui::TableNextColumn(); // Raw bytes display
        {
            int objsize = memory_object->GetSize();
            stringstream ss;
            ss << hex << setfill('0') << uppercase;
            for(int i = 0; i < objsize; i++) {
                int bval = (int)((u8*)&memory_object->bval)[i];
                if(memory_object->type == MemoryObject::TYPE_STRING) {
                    bval = memory_object->str.data[i];
                }
                ss << setw(2) << bval;
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
            ImGui::PushItemWidth(-FLT_MIN);
            if(ImGui::InputText("", &edit_buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
                parse_operand_expression = true;
            }
            if(parse_operand_expression && ParseOperandExpression(system, where)) {
                edit_mode = EDIT_NONE;
            }

            // when editing, we want this column to take the rest of the row
            goto end_table;
        } else {
            // TODO The line value will be used to index into the middle of data arrays, so that
            // multiple data listing items can show something like:
            // 
            // .DB $01, $02, $03,
            //     $04, $05, $06
            //     $07
            //
            string operand = memory_object->FormatOperandField(line, disassembler);
            ImGui::Text("%s", operand.c_str());
            if(hovered && ImGui::IsMouseDoubleClicked(0)) { // edit on double click
                EditOperandExpression(system, where);
            }
        }

        ImGui::TableNextColumn();
        if(edit_mode == EDIT_EOL_COMMENT) {
            if(started_editing) {
                ImGui::SetKeyboardFocusHere();
                started_editing = false;
            }
            ImGui::PushItemWidth(-FLT_MIN);
            if(ImGui::InputText("", &edit_buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
                system->SetComment(where, MemoryObject::COMMENT_TYPE_EOL, edit_buffer);
                edit_mode = EDIT_NONE;
            }
        } else {
            string eol_comment;
            system->GetComment(where, MemoryObject::COMMENT_TYPE_EOL, eol_comment); // TODO multiline
            if(eol_comment.size()) {
                ImGui::Text("; %s", eol_comment.c_str());
                if(hovered && ImGui::IsMouseDoubleClicked(0)) { // edit on double click
                    edit_buffer = eol_comment;
                    edit_mode = EDIT_EOL_COMMENT;
                    started_editing = true;
                }
            }
        }

end_table:
        ImGui::EndTable();
    }
}

void ListingItemPrimary::EditOperandExpression(shared_ptr<System>& system, GlobalMemoryLocation const& where)
{
    auto disassembler = system->GetDisassembler();
    if(auto memory_object = system->GetMemoryObject(where)) {
        switch(disassembler->GetAddressingMode(memory_object->code.opcode)) {
        case AM_IMPLIED:
        case AM_ACCUM:
            break;

        default:
            edit_buffer = memory_object->FormatOperandField(0, disassembler);
            edit_mode = EDIT_OPERAND_EXPRESSION;
            started_editing = true;
            break;
        }
    }
}

bool ListingItemPrimary::ParseOperandExpression(shared_ptr<System>& system, GlobalMemoryLocation const& where)
{
    if(!wait_dialog) {
        int errloc;

        auto expr = make_shared<Expression>();
        if(expr->Set(edit_buffer, parse_errmsg, errloc)) {
            // successfully parsed the expression, try to set it on the operand (where it will do semantic checking
            // and validate the expression is legal)
            if(!system->SetOperandExpression(where, expr, parse_errmsg)) {
                wait_dialog = true;
                stringstream ss;
                ss << "The operand expression is invalid: " << parse_errmsg;
                parse_errmsg = ss.str();
            } else {
                // the operand expression was set successfully
                parse_operand_expression = false;
                return true;
            }
        } else {
            wait_dialog = true;
            stringstream ss;
            ss << "The operand expression can't be parsed: " << parse_errmsg << " at position " << (errloc + 1);
            parse_errmsg = ss.str();
        }
    }

    if(wait_dialog) {
        if(MyApp::Instance()->OKPopup("Operand parse error", parse_errmsg)) {
            wait_dialog = false;
            parse_operand_expression = false;
            started_editing = true; // re-edit the expression
        }
    }

    return false;
}

void ListingItemPrimary::ResetOperandExpression(shared_ptr<System>& system, GlobalMemoryLocation const& where)
{
    system->CreateDefaultOperandExpression(where, false);
}

void ListingItemPrimary::NextLabelReference(shared_ptr<System>& system, GlobalMemoryLocation const& where)
{
    if(auto memory_region = system->GetMemoryRegion(where)) {
        memory_region->NextLabelReference(where);
    }
}


bool ListingItemPrimary::IsEditing() const 
{
    return edit_mode != EDIT_NONE;
}

void ListingItemLabel::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool selected, bool hovered)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }
    
    if(selected) {
        if(ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            editing = true;
            started_editing = true;
        } 

        if(ImGui::IsKeyPressed(ImGuiKey_R) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
            // show references to label
            auto wnd = Windows::References::CreateWindow(label);
            wnd->SetInitialDock(BaseWindow::DOCK_RIGHT);
            MyApp::Instance()->AddWindow(wnd);
        }
    }


    if(editing) {
        if(!selected || ImGui::IsKeyPressed(ImGuiKey_Escape)) { // must stop editing, discard
            editing = false;
        }
    }

    if(ImGui::BeginTable("listing_item_label", 2, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch);
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

bool ListingItemLabel::IsEditing() const 
{
    return editing;
}


}
