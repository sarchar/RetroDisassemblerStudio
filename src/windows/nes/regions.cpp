// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "util.h"

#include "systems/nes/memory.h"
#include "systems/nes/system.h"

#include "windows/nes/emulator.h"
#include "windows/nes/listing.h"
#include "windows/nes/project.h"
#include "windows/nes/regions.h"

using namespace std;

namespace Windows::NES {

REGISTER_WINDOW(MemoryRegions);

shared_ptr<MemoryRegions> MemoryRegions::CreateWindow()
{
    return CreateWindow(false, 0);
}

shared_ptr<MemoryRegions> MemoryRegions::CreateWindow(bool select_region, u32 filter_address)
{
    return make_shared<MemoryRegions>(select_region, filter_address);
}

MemoryRegions::MemoryRegions(bool _select_region, u32 _filter_address)
    : BaseWindow(), select_region(_select_region), filter_address(_filter_address), selected_row(-1)
{
    if(select_region) {
        SetDockable(false);
        SetPopup(true);
        SetNoScrollbar(true);
        SetTitle("Select Memory Region");
    } else {
        SetTitle("Memory Regions");
    }
    
    // create internal signals

    if(auto system = GetSystem()) {
        // grab a weak_ptr so we don't have to continually use dynamic_pointer_cast
        current_system = system;
    }
}

MemoryRegions::~MemoryRegions()
{
}

void MemoryRegions::Update(double deltaTime) 
{
}

void MemoryRegions::Render() 
{
    // All access goes through the system
    auto system = current_system.lock();
    if(!system) return;

    bool enter_pressed = false;
    if(select_region) {
        ImGui::PushItemWidth(-FLT_MIN);
        enter_pressed = ImGui::InputText("##select_region_name", &edit_buffer, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SetItemDefaultFocus();
        if(select_region_first_focus) {
            ImGui::SetKeyboardFocusHere(-1);
            if(ImGui::IsItemActive()) select_region_first_focus = false;
        }
    }

    int to_select_row = -1;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH 
            | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody
            | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_ScrollY;

    ImVec2 outer_size = ImGui::GetWindowSize();
    if(select_region) outer_size.y = ImGui::GetTextLineHeight() * 14;

    if (ImGui::BeginTable("MemoryRegionsTable", select_region ? 1 : 3, flags, outer_size)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        if(!select_region) {
            ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
        }

        for(int i = 0; i < system->GetNumMemoryRegions(); i++) {
            auto memory_region = system->GetMemoryRegionByIndex(i);

            // filter region list
            if(select_region) {
                // by containing address
                if(filter_address < memory_region->GetBaseAddress() 
                   || filter_address >= memory_region->GetEndAddress()) continue;

                // and by edit_buffer
                if(memory_region->GetName().find(edit_buffer) == string::npos) continue;
            }

            // default to selecting the first region but also keep the currently selected region
            if(to_select_row == -1 || selected_row == i) to_select_row = i;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // Create the hidden selectable item
            {
                ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                char buf[32];
                sprintf(buf, "##mrt_selectable_row%d", i);
                if(ImGui::Selectable(buf, selected_row == i, selectable_flags)) {
                    selected_row = i;
                    to_select_row = i;
                }

                if(ImGui::IsItemHovered()) {
                    if(!select_region && ImGui::IsMouseClicked(0)) {
                        if(auto wnd = GetMySystemInstance()->GetMostRecentListingWindow()) {
                            // build an address from the bank info
                            GlobalMemoryLocation loc;
                            memory_region->GetGlobalMemoryLocation(0, &loc);
                            wnd->GoToAddress(loc, true);
                        }
                    } else if(select_region && ImGui::IsMouseDoubleClicked(0)) {
                        enter_pressed = true;
                    }
                }
                ImGui::SameLine();
            }

            ImGui::Text("%s", memory_region->GetName().c_str());

            if(!select_region) {
                ImGui::TableNextColumn();
                ImGui::Text("$%04X", memory_region->GetBaseAddress());

                ImGui::TableNextColumn();
                ImGui::Text("$%04X", memory_region->GetRegionSize());
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar(2);

    // automatically select the region
    if(select_region) selected_row = to_select_row;

    if(select_region) {
        if(ImGui::Button("OK") || enter_pressed) {
            if(selected_row != -1) {
                region_selected->emit(system->GetMemoryRegionByIndex(selected_row));
                ClosePopup();
            }
        }

        ImGui::SameLine();
        if(ImGui::Button("Cancel")) {
            ClosePopup();
        }
    }
}

} //namespace Windows::NES

