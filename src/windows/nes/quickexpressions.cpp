// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 

#include "imgui.h"
#include "imgui_internal.h"

#include "util.h"

#include "systems/nes/memory.h"
#include "systems/nes/system.h"

#include "windows/main.h"
#include "windows/nes/project.h"
#include "windows/nes/quickexpressions.h"

using namespace std;

namespace Windows::NES {

REGISTER_WINDOW(QuickExpressions);

shared_ptr<QuickExpressions> QuickExpressions::CreateWindow()
{
    return make_shared<QuickExpressions>();
}

QuickExpressions::QuickExpressions()
    : BaseWindow(), selected_row(-1)
{
    SetTitle("Expressions");
    
    // create internal signals

    if(auto system = GetSystem()) {
        current_system = system;

        *current_system->new_quick_expression += [this](s64 expression_value, string const& expression_string) {
            expressions.push_back(QuickExpressionData{
                .expression_string = expression_string,
                .expression_value  = expression_value
            });
            need_resort = true;
        };
    }
}

QuickExpressions::~QuickExpressions()
{
}

void QuickExpressions::Update(double deltaTime) 
{
    if(need_reiterate) {
        Reiterate();
        need_reiterate = false;
        need_resort = true;
    }

    if(need_resort) {
        Resort();
        need_resort = false;
    }
}

void QuickExpressions::Render() 
{
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH 
            | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody
            | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_ScrollY 
            | ImGuiTableFlags_Sortable;

    ImVec2 outer_size = ImGui::GetWindowSize();

    if (ImGui::BeginTable("QuickExpressionsTable", 2, flags, outer_size)) {
        ImGui::TableSetupColumn("Expression", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
        ImGui::TableSetupColumn("Value"     , ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        
        // Sort our data (on the next frame) if sort specs have been changed!
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

        for(int i = 0; i < expressions.size(); i++) {
            auto& qe_data = expressions[i];
            ImGui::TableNextRow();

            // Create the hidden selectable item
            ImGui::TableNextColumn();
            {
                ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                char buf[32];
                sprintf(buf, "##qet_selectable_row%d", i);
                if(ImGui::Selectable(buf, selected_row == i, selectable_flags)) {
                    selected_row = i;
                }

                ImGui::SameLine();
            }

            ImGui::Text("%s", qe_data.expression_string.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("$%02X", qe_data.expression_value);
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar(2);
}

void QuickExpressions::Reiterate()
{
    expressions.clear();

    current_system->IterateQuickExpressions([this](s64 expression_value, string const& expression_string) {
        expressions.push_back(QuickExpressionData{
            .expression_string = expression_string,
            .expression_value  = expression_value
        });
    });
}

void QuickExpressions::Resort()
{
    // enum names are only sorted by by their name
    sort(expressions.begin(), expressions.end(), 
        [this](QuickExpressionData const& a, QuickExpressionData const& b)->bool {
            bool diff;

            if(sort_column == 0) { // sort on expression_string
                if(reverse_sort) diff = b.expression_string <= a.expression_string;
                else             diff = a.expression_string <= b.expression_string;
            } else if(sort_column == 1) { // sort on expression_value
                if(reverse_sort) diff = b.expression_value <= a.expression_value;
                else             diff = a.expression_value <= b.expression_value;
            }

            return diff;
        }
    );
}

} //namespace Windows::NES

