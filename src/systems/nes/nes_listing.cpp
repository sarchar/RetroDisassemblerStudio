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

void ListingItemUnknown::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool focused, bool selected, bool hovered)
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

void ListingItemBlankLine::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool focused, bool selected, bool hovered)
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

void ListingItemPrePostComment::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool focused, bool selected, bool hovered)
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

void ListingItemPrimary::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool focused, bool selected, bool hovered)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    auto memory_object = system->GetMemoryObject(where);
    if(!memory_object) return;
    auto disassembler = system->GetDisassembler();

    // only receive keyboard input if the window the listing item is in is in focus
    if(focused) {
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

        if(ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            edit_mode = EDIT_NONE;
        }
    }

    // losing selection can happen without focus
    if(!selected) {
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
            ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackAlways;
            auto cb = [this, &system](ImGuiInputTextCallbackData* data)->int {
                if(data->EventFlag != ImGuiInputTextFlags_CallbackAlways) return 0;
                if(!data->Buf) return 0;
                //cout << "CursorPos = " << data->CursorPos << " buf = " << data->Buf << endl;
                suggestions.clear();

                // naive filter: every keypress
                string bufstr(data->Buf);
                system->IterateLabels([this, &bufstr](shared_ptr<Label>& label) {
                    auto label_name = label->GetString();
                    //cout << "checking " << label_name << " find = " << label_name.find(bufstr) << endl;
                    if(label_name.find(bufstr) == 0) {
                        suggestions.push_back(label);
                    }
                });

                system->IterateDefines([this, &bufstr](shared_ptr<Define>& define) {
                    auto define_name = define->GetString();
                    if(define_name.find(bufstr) == 0) {
                        suggestions.push_back(define);
                    }
                });

                sort(suggestions.begin(), suggestions.end(), [](suggestion_type const& a, suggestion_type const& b) {
                    string a_str, b_str;
                    if(auto const label = get_if<shared_ptr<Label>>(&a)) {
                        a_str = (*label)->GetString();
                    } else if(auto const define = get_if<shared_ptr<Define>>(&a)) {
                        a_str = (*define)->GetString();
                    }
                    if(auto const label = get_if<shared_ptr<Label>>(&b)) {
                        b_str = (*label)->GetString();
                    } else if(auto const define = get_if<shared_ptr<Define>>(&b)) {
                        b_str = (*define)->GetString();
                    }
                            
                    return a_str <= b_str;
                });
                return 0;
            };

            auto wrapper = [](ImGuiInputTextCallbackData* data)->int {
                std::function<int(ImGuiInputTextCallbackData*)>* f = static_cast<std::function<int(ImGuiInputTextCallbackData*)>*>(data->UserData);
                return (*f)(data);
            };

            ImGui::PushItemWidth(-FLT_MIN);
            std::function<int(ImGuiInputTextCallbackData*)> fcb(cb);
            if(ImGui::InputText("", &edit_buffer, input_flags, wrapper, (void*)&fcb)) {
                parse_operand_expression = true;
            }

            if(started_editing) {
                ImGui::SetKeyboardFocusHere(-1);
            }

            if(ImGui::IsItemActivated()) {
                ImGui::OpenPopup("##suggestions");
                cout << "input text activated!" << endl;
                started_editing = false;
            }

            bool is_input_active = ImGui::IsItemActive();

            ImGui::SameLine();
            ImGui::SetNextWindowPos({ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y});
            ImGui::SetNextWindowSize({ImGui::GetItemRectSize().x, 0});
            if(ImGui::BeginPopup("##suggestions", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
                        | ImGuiWindowFlags_ChildWindow)) {
                //ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
                for(int i = 0; i < suggestions.size(); i++) {
                    stringstream ss;
                    if(auto label = get_if<shared_ptr<Label>>(&suggestions[i])) {
                        ss << (*label)->GetString();
                    } else if(auto define = get_if<shared_ptr<Define>>(&suggestions[i])) {
                        ss << (*define)->GetString(); 
                    }

                    string s = ss.str();
                    if(ImGui::Selectable(s.c_str())) {
                        ImGui::ClearActiveID();
                        edit_buffer = s;
                        parse_operand_expression = true;
                    }
                }

                if(parse_operand_expression || (!is_input_active && !ImGui::IsWindowFocused())) {
                    cout << "closing suggestions" << endl;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
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

    // if we're told to parse the operand expression, try to do so
    if(parse_operand_expression && ParseOperandExpression(system, where)) edit_mode = EDIT_NONE;
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
            cout << "OKPopup closed" << endl;
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

void ListingItemLabel::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool focused, bool selected, bool hovered)
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
