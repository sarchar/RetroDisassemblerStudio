// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#include <cassert>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "util.h"

#include "windows/main.h"
#include "windows/nes/emulator.h"
#include "windows/nes/listing.h"
#include "windows/nes/listingitems.h"
#include "windows/nes/references.h"

#include "systems/nes/comment.h"
#include "systems/nes/disasm.h"
#include "systems/nes/enum.h"
#include "systems/nes/expressions.h"
#include "systems/nes/label.h"
#include "systems/nes/memory.h"
#include "systems/nes/system.h"

using namespace std;

namespace Windows::NES {

unsigned long ListingItem::common_inner_table_flags = ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Resizable;

void ListingItemUnknown::Render(shared_ptr<Windows::NES::SystemInstance> const& system_instance, shared_ptr<System>& system, 
        GlobalMemoryLocation const& where, u32 flags, 
        bool focused, bool selected, bool hovered, postponed_changes& changes)
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

void ListingItemBlankLine::Render(shared_ptr<Windows::NES::SystemInstance> const& system_instance, shared_ptr<System>& system, 
        GlobalMemoryLocation const& where, u32 flags, 
        bool focused, bool selected, bool hovered, postponed_changes& changes)
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

static void SetupPrimaryTableColumns()
{
    float bp_size = ImGui::GetTextLineHeight();
    ImGui::TableSetupColumn("##Break"   , ImGuiTableColumnFlags_WidthFixed, bp_size);
    ImGui::TableSetupColumn("Address"   , ImGuiTableColumnFlags_WidthFixed, 92); // some saner default sizes, taken from the printout below
    ImGui::TableSetupColumn("Spacing0"  , ImGuiTableColumnFlags_WidthFixed, 31);
    ImGui::TableSetupColumn("Raw"       , ImGuiTableColumnFlags_WidthFixed, 88);
    ImGui::TableSetupColumn("Mnemonic"  , ImGuiTableColumnFlags_WidthFixed, 63);
    ImGui::TableSetupColumn("Operand"   , ImGuiTableColumnFlags_WidthFixed, 257);
    ImGui::TableSetupColumn("EOLComment", ImGuiTableColumnFlags_WidthStretch); // stretch comment to EOL

    // uncomment to print out column widths. TODO allow user to decide to save the widths as a global default
    //!if(selected && ImGui::IsMouseClicked(0)) {
    //!    ImGuiTable* table = ImGui::GetCurrentTable();
    //!    for(int i = 0; i < table->ColumnsCount; i++) {
    //!        ImGuiTableColumn& col = table->Columns[i];
    //!        cout << (float)col.WidthGiven << " ";
    //!    }
    //!    cout << endl;
    //!}
}

void ListingItemCommentOnly::Render(shared_ptr<Windows::NES::SystemInstance> const& system_instance, shared_ptr<System>& system, 
        GlobalMemoryLocation const& where, u32 flags, 
        bool focused, bool selected, bool hovered, postponed_changes& changes)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    auto memory_object = system->GetMemoryObject(where);
    if(!memory_object) return;
    auto comment = memory_object->GetComment(comment_type);
    if(!comment) return;

    // For EOL comments we use the same table name as Primary listing items
    // so that the columns align with the ListingItemPrimary items
    char const* table_name = 
        (comment_type == MemoryObject::COMMENT_TYPE_PRE) ? "listing_item_precomment"
            : (comment_type == MemoryObject::COMMENT_TYPE_POST) ? "listing_item_postcomment"
                : "listing_item_primary";

    // the listing_item_primary table has 7 columns, while the others have two
    int num_columns = (comment_type == MemoryObject::COMMENT_TYPE_EOL) ? 7 : 2;

    if(ImGui::BeginTable(table_name, num_columns, table_flags)) {
        // set up the columns and select the final column
        switch(comment_type) {
        case MemoryObject::COMMENT_TYPE_PRE:
        case MemoryObject::COMMENT_TYPE_POST:
            ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthFixed, 4.0f);
            ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            
            ImGui::TableNextColumn();
            ImGui::Text("        ");
    
            ImGui::TableNextColumn();
            break;

        case MemoryObject::COMMENT_TYPE_EOL:
            SetupPrimaryTableColumns();
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); // Break
            ImGui::TableNextColumn(); // Address
            ImGui::TableNextColumn(); // Spacing0
            ImGui::TableNextColumn(); // Raw
            ImGui::TableNextColumn(); // Mnemonic
            ImGui::TableNextColumn(); // Operand
            ImGui::TableNextColumn(); // EOLComment
            break;

        default:
            assert(false); // unimplemented
            break;
        }

        RenderCommentContent(system_instance, comment, comment_line, changes);

        ImGui::EndTable();
    }
}

void ListingItemCommentOnly::RenderCommentContent(shared_ptr<Windows::NES::SystemInstance> const& system_instance,
        shared_ptr<BaseComment> const& comment, int comment_line, ListingItem::postponed_changes& changes)
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,-1));
    ImGui::Text("; ");
    for(int j = 0; j < comment->GetLineItemCount(comment_line); j++) {
        string item;

        auto t = comment->FormatLineItem(comment_line, j, item, false);
        switch(t) {
        case BaseComment::LINE_ITEM_TYPE::STRING:
            break;
        case BaseComment::LINE_ITEM_TYPE::EXPRESSION:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(0, 0, 255, 255));
            break;
        case BaseComment::LINE_ITEM_TYPE::ERROR:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(255, 0, 0, 255));
            break;
        }

        ImGui::SameLine();
        ImGui::Text("%s", item.c_str());

        switch(t) {
        case BaseComment::LINE_ITEM_TYPE::ERROR:
        case BaseComment::LINE_ITEM_TYPE::EXPRESSION: {
            ImGui::PopStyleColor(1);
            ImVec2 line_end = ImGui::GetItemRectMax();
            line_end.y -= 1;
            ImVec2 line_start = line_end;
            line_start.x = ImGui::GetItemRectMin().x;
            ImGui::GetWindowDrawList()->AddLine(line_start, line_end, ImColor(0, 0, 0, 255));
            break;
        }
        default:
            break;
        }

        if((t == BaseComment::LINE_ITEM_TYPE::EXPRESSION) && ImGui::IsItemHovered()) {
            s64 long_address;
            comment->FormatLineItem(comment_line, j, item, true, &long_address);
            if(ImGui::IsMouseDoubleClicked(0)) {
                auto listing = system_instance->GetMostRecentListingWindow();
                if(listing) {
                    GlobalMemoryLocation dest;
                    system_instance->GetSystem()->GetLocationFromLongAddress(long_address, dest);
                    changes.push_back([dest, listing]() {
                        listing->GoToAddress(dest, true);
                    });
                }
            }
            ImGui::SetTooltip("%s", item.c_str());
        }
    }
    ImGui::PopStyleVar(1);
}

void ListingItemPrimary::Render(shared_ptr<Windows::NES::SystemInstance> const& system_instance, shared_ptr<System>& system, 
        GlobalMemoryLocation const& where, u32 flags, 
        bool focused, bool selected, bool hovered, postponed_changes& changes)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    auto memory_object = system->GetMemoryObject(where);
    if(!memory_object) return;
    auto disassembler = system->GetDisassembler();
    auto comment = memory_object->GetComment(MemoryObject::COMMENT_TYPE_EOL);

    // only receive keyboard input if the window the listing item is in is in focus
    // and only respond on the primary line
    if(focused && line == 0) {
        if(selected && edit_mode == EDIT_NONE) {
            if(ImGui::IsKeyPressed(ImGuiKey_Enter)) { // edit the operand expression
                EditOperandExpression(system, where);
            } else if(ImGui::IsKeyPressed(ImGuiKey_Backspace)) { // clear labels
                ResetOperandExpression(system, where);
            } else if(ImGui::IsKeyPressed(ImGuiKey_A)) { // next label
                NextLabelReference(system, where);
            } else if(ImGui::IsKeyPressed(ImGuiKey_Delete)) { // delete data type
                // we have to be cautious about capturing 'this', as the listing item could be deleted when something else
                // recreates the listing items for a memory object. so we'll capture a copy of the label instead
                changes.push_back([system, where]() {
                    system->MarkMemoryAsUndefined(where, 1); // can specify 1 for size instead of GetSize()
                });
            }
        } 

        if(ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            edit_mode = EDIT_NONE;
        }
    }

    // losing selection can happen without focus
    if(!selected) edit_mode = EDIT_NONE;

    // make sure the suggestion popup is closed when not editing
    if(edit_mode == EDIT_NONE && ImGui::IsPopupOpen("##suggestion")) {
        if(ImGui::BeginPopup("##suggestions")) {
            // kinda feels like a ClosePopup() function should exist
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    if(ImGui::BeginTable("listing_item_primary", 7, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        SetupPrimaryTableColumns();

        ImGui::TableNextRow();
    
        ImGui::TableNextColumn();   // same color as the address field
        if(line == 0) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, (ImU32)ImColor(200, 200, 200, (selected || hovered) ? 128 : 255));
            Windows::NES::SystemInstance::breakpoint_list_t bplist = system_instance->GetBreakpointsAt(where);

            // extend bplist with the non-bank-specific breakpoints
            Windows::NES::SystemInstance::breakpoint_list_t bplist2 = system_instance->GetBreakpointsAt(where.address);
            bplist.insert(bplist.end(), bplist2.begin(), bplist2.end());

            bool show_disabled_bp = false;
            std::shared_ptr<BreakpointInfo> bpi = nullptr;

            for(auto& bpiter : bplist) {
                if(bpiter->enabled && bpiter->break_execute) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, (ImU32)ImColor(232, 0, 0, (selected || hovered) ? 128 : 255));
                    ImGui::Text(" X");
                    show_disabled_bp = false;
                    bpi = bpiter;
                    break;
                } else {
                    show_disabled_bp = true;
                }
            }

            if(show_disabled_bp) {
                bpi = bplist[0];
                ImGui::TextDisabled(" X");
            } else if(bplist.size() == 0) {
                ImGui::Text("  ");
            }

            if((ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) || (focused && selected && ImGui::IsKeyPressed(ImGuiKey_F9))) {
                if(bpi) {
                    if(bpi->has_bank) {
                        system_instance->ClearBreakpoint(where, bpi);
                    } else {
                        system_instance->ClearBreakpoint(where.address, bpi);
                    }
                } else {
                    // create new breakpoint here
                    bpi = make_shared<BreakpointInfo>();
                    bpi->address = where;
                    bpi->has_bank = true;
                    bpi->enabled = true;
                    bpi->break_execute = true;
                    system_instance->SetBreakpoint(where, bpi);
                }
            }
        }

        ImGui::TableNextColumn();
        if(line == 0) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, (ImU32)ImColor(200, 200, 200, (selected || hovered) ? 128 : 255));
            ImGui::Text("$%02X:%04X", where.prg_rom_bank, where.address);
        }

        ImGui::TableNextColumn(); // spacing

        ImGui::TableNextColumn(); // Raw bytes display
        if(line == 0) { // TODO this will need improvement one day
            int objsize = memory_object->GetSize();
            stringstream ss;
            ss << hex << setfill('0') << uppercase;
            for(int i = 0; i < objsize; i++) {
                if(memory_object->backed) {
                    int bval = (int)memory_object->data_ptr[i];
                    ss << setw(2) << bval;
                } else {
                    ss << "??";
                }
                if(i != (objsize - 1)) ss << " ";
            }

            ImGui::Text("%s", ss.str().c_str());
        }

        ImGui::TableNextColumn();
        if(line == 0) ImGui::Text("%s", memory_object->FormatInstructionField(disassembler).c_str());

        ImGui::TableNextColumn();
        if(line == 0) {
            if(edit_mode == EDIT_OPERAND_EXPRESSION) {
                // when editing, we want this column to take the rest of the row
                RenderEditOperandExpression(system);
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
                if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) { // edit on double click
                    EditOperandExpression(system, where);
                }
            }
        }

        // Comment
        ImGui::TableNextColumn();
        if(comment) ListingItemCommentOnly::RenderCommentContent(system_instance, comment, 0, changes);

end_table:
        ImGui::EndTable();
    }

    // if we're told to parse the operand expression, try to do so
    if(do_parse_operand_expression && ParseOperandExpression(system, where)) edit_mode = EDIT_NONE;
}

void ListingItemPrimary::EditOperandExpression(shared_ptr<System>& system, GlobalMemoryLocation const& where)
{
    auto disassembler = system->GetDisassembler();
    expression_value = -1;

    if(auto memory_object = system->GetMemoryObject(where)) {
        bool editable = memory_object->type == MemoryObject::TYPE_CODE
                        || memory_object->type == MemoryObject::TYPE_BYTE
                        || memory_object->type == MemoryObject::TYPE_WORD;
        if(editable) {
            edit_buffer = memory_object->FormatOperandField(0, disassembler);
            edit_mode = EDIT_OPERAND_EXPRESSION;
            started_editing = true;

            // determine expression value for the quick expression search
            if(memory_object->GetSize() == 3) {
                expression_value = (u16)memory_object->data_ptr[1] | ((u16)memory_object->data_ptr[2] << 8);
            } else if(memory_object->GetSize() == 2) {
                expression_value = memory_object->data_ptr[1];
            }
        }
    }

    RecalculateSuggestions(system);
    deselect_input = false;
}

void ListingItemPrimary::RecalculateSuggestions(shared_ptr<System>& system)
{
    suggestions.clear();
    suggestion_start = -1;

    // find the start of the word
    int i = edit_buffer.size();
    while(i > 0 && edit_buffer[i] != '.' && (isalnum(edit_buffer[i-1]) || edit_buffer[i-1] == '_')) --i;

    // labels and defines can't start with a digit, but we
    // TODO might have to allow digits when we have expression completions
    if(i < edit_buffer.size() && isdigit(edit_buffer[i])) return;

    string bufstr = edit_buffer.substr(i);
    suggestion_start = i;

    system->IterateLabels([this, &bufstr](shared_ptr<Label>& label) {
        auto label_name = label->GetString();
        if(label_name.find(bufstr) == 0) {
            suggestions.push_back(label);
        }
    });
    
    system->IterateDefines([this, &bufstr](shared_ptr<Define>& define) {
        auto define_name = define->GetName();
        if(define_name.find(bufstr) == 0) {
            suggestions.push_back(define);
        }
    });

    system->IterateEnumElements([this, &bufstr](shared_ptr<EnumElement> const& ee) {
        if(ee->GetName().find(bufstr) == 0 || ee->GetFormattedName("_").find(bufstr) == 0) {
            suggestions.push_back(ee);
        }
    });

    system->IterateQuickExpressionsByValue([this](string const& expression_string) {
        if(expression_string.find(edit_buffer) == 0) {
            suggestions.push_back(expression_string);
        }
    }, expression_value);
    
    sort(suggestions.begin(), suggestions.end(), [](suggestion_type const& a, suggestion_type const& b) {
        string a_str, b_str;

        if(auto const label = get_if<shared_ptr<Label>>(&a)) {
            a_str = (*label)->GetString();
        } else if(auto const define = get_if<shared_ptr<Define>>(&a)) {
            a_str = (*define)->GetName();
        } else if(auto const ee = get_if<shared_ptr<EnumElement>>(&a)) {
            a_str = (*ee)->GetName();
        } else if(auto const qe = get_if<string>(&a)) {
            a_str = *qe;
        }

        if(auto const label = get_if<shared_ptr<Label>>(&b)) {
            b_str = (*label)->GetString();
        } else if(auto const define = get_if<shared_ptr<Define>>(&b)) {
            b_str = (*define)->GetName();
        } else if(auto const ee = get_if<shared_ptr<EnumElement>>(&b)) {
            b_str = (*ee)->GetName();
        } else if(auto const qe = get_if<string>(&b)) {
            b_str = *qe;
        }
                
        return a_str <= b_str;
    });
}

int ListingItemPrimary::EditOperandExpressionTextCallback(void* _data)
{
    ImGuiInputTextCallbackData* data = static_cast<ImGuiInputTextCallbackData*>(_data);

    if(data->EventFlag != ImGuiInputTextFlags_CallbackAlways) return 0;
    if(!data->Buf) return 0;
    //cout << "CursorPos = " << data->CursorPos << " buf = " << data->Buf << endl;

    // when we make changes to the input text, it doesn't make sense to select the entire text again
    if(deselect_input) {
        data->SelectionStart = data->SelectionEnd;
        deselect_input = false;
    }
    return 0;
}

// this thread was full of useful information that made this suggestion popup possible
// still has some quirks though.
// https://github.com/ocornut/imgui/issues/718
void ListingItemPrimary::RenderEditOperandExpression(shared_ptr<System>& system)
{
    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackAlways;

    // wrapper function calls EditOperandExpressionTextCallback
    typedef function<int(void*)> cb_func;
    cb_func cb = std::bind(&ListingItemPrimary::EditOperandExpressionTextCallback, this, placeholders::_1);

    auto wrapped_cb = [](ImGuiInputTextCallbackData* data)->int {
        cb_func* func = static_cast<cb_func*>(data->UserData);
        return (*func)((void*)data);
    };

    ImGui::PushItemWidth(-FLT_MIN); // render as wide as possible

    string tmp_buffer = edit_buffer; // edit on a duplicate buffer, and check for changes
    if(ImGui::InputText("", &tmp_buffer, input_flags, wrapped_cb, (void*)&cb)) {
        do_parse_operand_expression = true;
    }
    
    // if we just started editing, focus on the input text item
    if(started_editing) {
        ImGui::SetKeyboardFocusHere(-1);
        started_editing = false;
    }

    // once the item becomes activated, open the suggestions popup, which will stay open for the duration of the edit
    if(ImGui::IsItemActivated()) {
        if(!ImGui::IsPopupOpen("##suggestions")) ImGui::OpenPopup("##suggestions");
    }

    // react to the text changing by changing the suggestions. cursor position is set in the callback
    if(tmp_buffer != edit_buffer) {
        edit_buffer = tmp_buffer;
        // TODO the inefficient version here loops over every name every time the buffer changes, but we could improve
        // the situation by only doing that with backspace but filtering the set more when characters are added.
        // we'll see in the future how performance looks when we have lots of items
        RecalculateSuggestions(system);
    }
    
    // true when the text input is being edited
    bool is_input_active = ImGui::IsItemActive();
    
    // render the suggestions popup
    float height = ImGui::GetItemRectSize().y * 8; // set to 0 for variable height
    ImGui::SetNextWindowPos({ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y}); // position to the bottom left corner of the inputtext
    ImGui::SetNextWindowSize({ImGui::GetItemRectSize().x, height});                  // set the width equal to the input text
                                                                                     //
    if(ImGui::BeginPopup("##suggestions", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_ChildWindow)) {

        for(int i = 0; i < suggestions.size(); i++) {
            stringstream disp, repl;
            bool replace_all = false;
            if(auto label = get_if<shared_ptr<Label>>(&suggestions[i])) {
                disp << (*label)->GetString() << " (label)";  
                repl << (*label)->GetString();
            } else if(auto define = get_if<shared_ptr<Define>>(&suggestions[i])) {
                disp << (*define)->GetName() << " (define)"; 
                repl << (*define)->GetName(); 
            } else if(auto ee = get_if<shared_ptr<EnumElement>>(&suggestions[i])) {
                auto e = (*ee)->parent_enum.lock();
                if(!e) continue;
                disp << (*ee)->GetFormattedName("_") << " (enum)";
                repl << (*ee)->GetFormattedName("_");
            } else if(auto qe = get_if<string>(&suggestions[i])) {
                disp << *qe;
                repl << *qe;
                replace_all = true;
            }
    
            // TODO we can use tables to make a selectable row and then colorize
            // the quick expressions a different color
            string display_str = disp.str();
            if(ImGui::Selectable(display_str.c_str())) {
                ImGui::ClearActiveID();

                if(suggestion_start != -1 || replace_all) {
                    if(replace_all) {
                        edit_buffer = repl.str();
                    } else {
                        edit_buffer = edit_buffer.substr(0, suggestion_start) + repl.str();
                    }
                    RecalculateSuggestions(system);

                    // Close the popup, and restart editing which will refocus on the inputtext
                    // but we want to keep the cursor at the end of the line, not reselect everything
                    ImGui::CloseCurrentPopup();
                    started_editing = true;
                    deselect_input = true;
                }
            }
        }
    
        if(do_parse_operand_expression) {
            cout << "closing suggestions" << endl;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

bool ListingItemPrimary::ParseOperandExpression(shared_ptr<System>& system, GlobalMemoryLocation const& where)
{
    if(!wait_dialog) {
        int errloc;

        auto expr = make_shared<Systems::NES::Expression>();
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
                do_parse_operand_expression = false;
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
        if(GetMainWindow()->OKPopup("Operand parse error", parse_errmsg)) {
            wait_dialog = false;
            do_parse_operand_expression = false;
            started_editing = true; // re-edit the expression
        }
    }

    return false;
}

void ListingItemPrimary::ResetOperandExpression(shared_ptr<System>& system, GlobalMemoryLocation const& where)
{
    auto det_func = [](u32, System::finish_default_operand_expression_func finish_expression) { 
        finish_expression(nullopt); // specifically don't set a label
    };
    system->CreateDefaultOperandExpression(where, false, det_func);
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

void ListingItemLabel::Render(shared_ptr<Windows::NES::SystemInstance> const& system_instance, shared_ptr<System>& system, 
        GlobalMemoryLocation const& where, u32 flags, 
        bool focused, bool selected, bool hovered, postponed_changes& changes)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }
    
    if(focused && selected) {
        if(ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            editing = true;
            started_editing = true;
        } 

        if(ImGui::IsKeyPressed(ImGuiKey_R) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
            // show references to label
            auto wnd = Windows::NES::References::CreateWindow(label);
            wnd->SetInitialDock(Windows::BaseWindow::DOCK_RIGHTBOTTOM);
            system_instance->AddChildWindow(wnd);
        }

        if(ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            // we have to be cautious about capturing 'this', as the listing item could be deleted when something else
            // recreates the listing items for a memory object. so we'll capture a copy of the label instead
            auto lcopy = label;
            changes.push_back([system, lcopy]() {
                system->DeleteLabel(lcopy);
            });
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
