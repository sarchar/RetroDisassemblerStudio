#include <iomanip>
#include <iostream>
#include <sstream>

#include "imgui.h"
#include "imgui_internal.h"

#include "main.h"
#include "windows/nes/listing.h"
#include "windows/nes/defines.h"
#include "systems/nes/nes_defines.h"
#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_project.h"
#include "systems/nes/nes_system.h"

using namespace std;

namespace NES {

namespace Windows {

shared_ptr<Defines> Defines::CreateWindow()
{
    return make_shared<Defines>();
}

Defines::Defines()
    : BaseWindow("NES::Defines"), selected_row(-1), force_resort(true), force_reiterate(true), case_sensitive_sort(false)
{
    SetTitle("Defines");
    
    // create internal signals

    if(auto system = (MyApp::Instance()->GetProject()->GetSystem<System>())) {
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

void Defines::UpdateContent(double deltaTime) 
{
}

void Defines::RenderContent() 
{
    // All access goes through the system
    auto system = current_system.lock();
    if(!system) return;

    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 0));

        bool need_pop = false;
        if(case_sensitive_sort) {
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(255, 0, 0));
            need_pop = true;
        }

        if(ImGui::Button("I")) {
            case_sensitive_sort = !case_sensitive_sort;
            force_resort = true;
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Case Sensitive Sort");
        if(need_pop) ImGui::PopStyleColor(1);

        ImGui::SameLine();
        // TODO htf do you right align buttons?
        //ImGui::PushItemWidth(ImGui::GetFontSize() * -12);
        if(ImGui::Button("+", ImVec2(/* ImGui::GetFontSize() * -2 */ 0, 0))) {
            command_signal->emit(shared_from_this(), "CreateNewDefine", nullptr);
        }

        ImGui::PopStyleVar(1);

        ImGui::Separator();
    }

    // rebuild the defines list
    if(force_reiterate) {
        defines.clear();
        auto cb = [this](shared_ptr<Define>& define)->void {
            defines.push_back(define);
        };
        system->IterateDefines(&cb);
        force_reiterate = false;
    }

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
        ImGui::TableSetupColumn("Value"     , ImGuiTableColumnFlags_WidthStretch                                    , 0.0f, 2);
        ImGui::TableSetupColumn("RRefs"     , ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort     , 0.0f, 3);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Sort our data if sort specs have been changed!
        if(ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
            if(force_resort || sort_specs->SpecsDirty) {
                sort(defines.begin(), defines.end(), [&](weak_ptr<Define>& a, weak_ptr<Define>& b)->bool {
                    const ImGuiTableColumnSortSpecs* spec = &sort_specs->Specs[0];
                    bool diff;
                    if(auto bp = b.lock()) {
                        string bstr = bp->GetString();
                        if(!case_sensitive_sort) bstr = strlower(bstr);
                        auto bexpr = bp->GetExpressionString();
                        auto bval = bp->Evaluate();
                        if(auto ap = a.lock()) {
                            string astr = ap->GetString();
                            if(!case_sensitive_sort) astr = strlower(astr);
                            auto aexpr = bp->GetExpressionString();
                            auto aval = ap->Evaluate();
                            if(spec->ColumnUserID == 0) {
                                diff = std::tie(astr, aexpr, aval) <= std::tie(bstr, bexpr, bval); 
                            } else if(spec->ColumnUserID == 1) {
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
                    if(spec->SortDirection == ImGuiSortDirection_Descending) diff = !diff;
                    return diff;
                });

                sort_specs->SpecsDirty = false;
            }
        }

        ImGuiListClipper clipper;
        u32 total_defines = defines.size(); //memory_region->GetTotalListingItems();
        //cout << "total_defines = 0x" << hex << total_defines << endl;
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

                    if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        // TODO do something when define is double clicked (edit?)
                        //!if(auto wnd = MyApp::Instance()->FindMostRecentWindow<Listing>()) {
                        //!    // build an address from the bank info
                        //!    wnd->GoToAddress(label->GetMemoryLocation());
                        //!}
                    }
                    ImGui::SameLine();
                }

                // And the define name in the same column
                ImGui::Text("%s", define->GetString().c_str());

                // Expression
                ImGui::TableNextColumn();
                ImGui::Text("%s", define->GetExpressionString().c_str());

                // Value
                ImGui::TableNextColumn();
                ImGui::Text("$%X", define->Evaluate());

                // Reverse refs
                ImGui::TableNextColumn();
                ImGui::Text("%d", define->GetNumReverseReferences());
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar(2);
}

void Defines::DefineCreated(shared_ptr<Define> const& define)
{
    defines.push_back(define);
    force_resort = true;
}

} //namespace Windows
} //namespace NES

