// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#include <iomanip>
#include <iostream>
#include <sstream>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "util.h"

#include "systems/nes/defines.h"
#include "systems/nes/memory.h"
#include "systems/nes/system.h"

#include "windows/nes/defines.h"
#include "windows/nes/emulator.h"
#include "windows/nes/listing.h"
#include "windows/nes/project.h"
#include "windows/nes/references.h"

using namespace std;

namespace Windows::NES {

REGISTER_WINDOW(Defines);

shared_ptr<Defines> Defines::CreateWindow()
{
    return make_shared<Defines>();
}

Defines::Defines()
{
    SetTitle("Defines");
    
    // create internal signals

    if(auto system = GetSystem()) {
        // grab a weak_ptr so we don't have to continually use dynamic_pointer_cast
        current_system = system;

        // watch for new defines
        define_created_connection = 
            system->define_created->connect(std::bind(&Defines::DefineCreated, this, placeholders::_1));
    }
}

Defines::~Defines()
{
}

void Defines::Highlight(std::shared_ptr<Define>& target)
{
    highlight = target;

    // TODO we have to loop over all defines, and find the index of the one that matches target
    // and then set up the clipper to scroll to that target.
    cout << "[Defines::Highlight] TODO select " << target->GetName() << endl;
    highlight = nullptr;
}

void Defines::Resort()
{
    if(sort_column == -1) return;

    sort(defines.begin(), defines.end(), [&](weak_ptr<Define>& a, weak_ptr<Define>& b)->bool {
        bool diff;

        if(auto bp = b.lock()) {
            string bstr = bp->GetName();
            if(!case_sensitive_sort) bstr = strlower(bstr);
            auto bexpr = bp->GetExpressionString();
            auto bval = bp->Evaluate();
            if(auto ap = a.lock()) {
                string astr = ap->GetName();
                if(!case_sensitive_sort) astr = strlower(astr);
                auto aexpr = bp->GetExpressionString();
                auto aval = ap->Evaluate();
                if(sort_column == 0) {
                    diff = std::tie(astr, aexpr, aval) <= std::tie(bstr, bexpr, bval); 
                } else if(sort_column == 1) {
                    diff = std::tie(aexpr, aval, astr) <= std::tie(bexpr, bval, bstr); 
                } else {
                    diff = std::tie(aval, astr, aexpr) <= std::tie(bval, bstr, bexpr); 
                } 
            } else {
                diff = false;
            }
        } else {
            diff = true;
        }

        if(reverse_sort) diff = !diff;

        return diff;
    });
}

void Defines::DeleteDefine(int row)
{
    if(row >= 0 && row < defines.size()) {
        auto define = defines[row].lock();
        if(define) {
            if(define->GetNumReverseReferences()) {
                wait_dialog_message = "The define is in use and cannot be deleted";
                wait_dialog = true;
                return;
            }
            GetSystem()->DeleteDefine(define);
        }
        defines.erase(defines.begin() + row);
    }
}

void Defines::CheckInput()
{
    if(ImGui::IsKeyPressed(ImGuiKey_Delete)) DeleteDefine(selected_row);
}

void Defines::Update(double deltaTime) 
{
    // rebuild the defines list
    if(need_reiterate) {
        defines.clear();
        GetSystem()->IterateDefines([this](shared_ptr<Define>& define)->void {
            defines.push_back(define);
        });
        need_reiterate = false;
        need_resort = true;
    }

    if(need_resort) {
        Resort();
        need_resort = false;
    }
}

void Defines::Render() 
{
    if(wait_dialog) {
        if(GetMainWindow()->OKPopup("Define error", wait_dialog_message)) {
            wait_dialog = false;
            started_editing = true; // re-edit whatever
        }
    }

    RenderToolBar();

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    static ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuterH 
            | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody
            | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Sortable
            | ImGuiTableFlags_ScrollY;

    ImVec2 outer_size = ImGui::GetWindowSize();
    outer_size.x -= 12;

    if(ImGui::BeginTable("DefinesTable", 4, flags, outer_size)) {
        ImGui::TableSetupColumn("Name"      , ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 0.0f, 0);
        ImGui::TableSetupColumn("Expression", ImGuiTableColumnFlags_WidthStretch                                    , 0.0f, 1);
        ImGui::TableSetupColumn("Value"     , ImGuiTableColumnFlags_WidthFixed                                      , 0.0f, 2);
        ImGui::TableSetupColumn("RRefs"     , ImGuiTableColumnFlags_WidthFixed   | ImGuiTableColumnFlags_NoSort     , 0.0f, 3);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Sort our data if sort specs have been changed!
        if(ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs(); sort_specs && sort_specs->SpecsDirty) {
            if(auto spec = &sort_specs->Specs[0]) {
                sort_column = spec->ColumnUserID;
                reverse_sort = (spec->SortDirection == ImGuiSortDirection_Descending);
            } else { // no sort!
                sort_column = -1;
                reverse_sort = false;
            }

            need_resort = true;
            sort_specs->SpecsDirty = false;
        }

        ImGuiListClipper clipper;
        u32 total_defines = defines.size();
        clipper.Begin(total_defines);

        while(clipper.Step()) {
            auto it = defines.begin() + clipper.DisplayStart;
            for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row, ++it) {
                auto define = (*it).lock();
                while(!define && it != defines.end()) {
                    defines.erase(it);
                    define = (*it).lock();
                }

                if(it == defines.end()) break;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // Create the hidden selectable item
                {
                    ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                    char buf[32];
                    sprintf(buf, "##lt_selectable_row%d", row);
                    if(ImGui::Selectable(buf, selected_row == row, selectable_flags)) {
                        selected_row = row;
                    }

                    if(ImGui::IsItemHovered()) {
                        context_row = row;

                        if(ImGui::IsMouseClicked(1)) ImGui::OpenPopup("define_context_menu");
                    }
                    ImGui::SameLine();
                }

                // Name
                ImGui::Text("%s", define->GetName().c_str());

                // Expression
                RenderExpressionColumn(define);

                // Value
                ImGui::TableNextColumn();
                ImGui::Text("$%X", define->Evaluate());

                // Reverse refs
                ImGui::TableNextColumn();
                ImGui::Text("%d", define->GetNumReverseReferences());
            }
        }

        RenderCreateNewDefineRow();

        ImGui::EndTable();
    }

    ImGui::PopStyleVar(2);

    if(ImGui::BeginPopupContextItem("define_context_menu")) {
        if(ImGui::MenuItem("View References")) {
            auto wnd = References::CreateWindow(defines[context_row].lock());
            wnd->SetInitialDock(BaseWindow::DOCK_RIGHTTOP);
            GetMySystemInstance()->AddChildWindow(wnd);
        }
        if(ImGui::MenuItem("Delete Define")) {
            DeleteDefine(context_row);
        }
        ImGui::EndPopup();
    }
}

void Defines::RenderToolBar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 0));

    bool need_pop = false;
    if(case_sensitive_sort) {
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(255, 0, 0));
        need_pop = true;
    }

    if(ImGui::Button("I")) {
        case_sensitive_sort = !case_sensitive_sort;
        need_resort = true;
    }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Case Sensitive Sort");
    if(need_pop) ImGui::PopStyleColor(1);

    ImGui::SameLine();
    // TODO htf do you right align buttons?
    //ImGui::PushItemWidth(ImGui::GetFontSize() * -12);
    if(ImGui::Button("+", ImVec2(/* ImGui::GetFontSize() * -2 */ 0, 0))) {
        creating_new_define = true;
        started_editing = true;
        edit_buffer = "";
    }

    ImGui::PopStyleVar(1);

    ImGui::Separator();
}


void Defines::RenderCreateNewDefineRow()
{
    // Create the <New Define> row
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    if(!creating_new_define) {
        ImGui::TextDisabled("<New Define>");
        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            creating_new_define = true;
            started_editing = true;
            edit_buffer = "";
        }

        return;
    }

    ImGui::PushItemWidth(-FLT_MIN);
    bool enter_pressed = ImGui::InputText("##create_define", &edit_buffer, ImGuiInputTextFlags_EnterReturnsTrue);

    if(started_editing) {
        ImGui::SetKeyboardFocusHere(-1);

        // wait until item is activated
        if(ImGui::IsItemActive()) started_editing = false;
    } else if(!ImGui::IsItemActive() && !enter_pressed) { // check if item lost activation. stop editing without saving
        creating_new_define = false;
        return;
    }

    if(wait_dialog || !enter_pressed) return;

    string errmsg;
    auto new_define = GetSystem()->CreateDefine(edit_buffer, errmsg); // will trigger the DefineCreated callback
    if(!new_define) {
        wait_dialog = true;
        wait_dialog_message = "Could not create define: " + errmsg;
        return;
    }

    // switch to editing the expression immediately
    creating_new_define = false;
    editing_expression = true;
    edit_buffer = "0";
    edit_define = new_define;
    started_editing = true;
}

void Defines::RenderExpressionColumn(shared_ptr<Define> const& define)
{
    ImGui::TableNextColumn();

    if(!(editing_expression && define == edit_define)) {
        auto expression_string = define->GetExpressionString();

        ImGui::Text("%s", expression_string.c_str());
        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            editing_expression = true;
            started_editing = true;
            edit_buffer = expression_string;
            edit_define = define;
        }

        return;
    }

    ImGui::PushItemWidth(-FLT_MIN);
    bool enter_pressed = ImGui::InputText("##edit_expression", &edit_buffer, ImGuiInputTextFlags_EnterReturnsTrue);

    if(started_editing) {
        ImGui::SetKeyboardFocusHere(-1);

        // wait until item is activated
        if(ImGui::IsItemActive()) started_editing = false;
    } else if(!ImGui::IsItemActive() && !enter_pressed) { // check if item lost activation. stop editing without saving
        editing_expression = false;
        return;
    }

    if(wait_dialog || !enter_pressed) return;

    string errmsg;
    if(!edit_define->SetExpression(edit_buffer, errmsg)) {
        wait_dialog = true;
        wait_dialog_message = "Could not set expression: " + errmsg;
        return;
    }

    editing_expression = false;
    need_resort = true;
}

void Defines::DefineCreated(shared_ptr<Define> const& define)
{
    defines.push_back(define);
    need_reiterate = true;
}

} //namespace Windows::NES

