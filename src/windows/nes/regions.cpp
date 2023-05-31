
#include "imgui.h"
#include "imgui_internal.h"

#include "util.h"

#include "systems/nes/memory.h"
#include "systems/nes/system.h"

#include "windows/nes/emulator.h"
#include "windows/nes/listing.h"
#include "windows/nes/project.h"
#include "windows/nes/regions.h"

using namespace std;

namespace Windows::NES {

shared_ptr<MemoryRegions> MemoryRegions::CreateWindow()
{
    return make_shared<MemoryRegions>();
}

MemoryRegions::MemoryRegions()
    : BaseWindow("NES::MemoryRegions"), selected_row(-1)
{
    SetTitle("Memory Regions");
    
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

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH 
            | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody
            | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_ScrollY;

    ImVec2 outer_size = ImGui::GetWindowSize();

    if (ImGui::BeginTable("MemoryRegionsTable", 3, flags, outer_size)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for(int i = 0; i < system->GetNumMemoryRegions(); i++) {
            auto memory_region = system->GetMemoryRegionByIndex(i);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // Create the hidden selectable item
            {
                ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                char buf[32];
                sprintf(buf, "##mrt_selectable_row%d", i);
                if(ImGui::Selectable(buf, selected_row == i, selectable_flags)) {
                    selected_row = i;
                }

                if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
                    if(auto wnd = GetMySystemInstance()->GetMostRecentListingWindow()) {
                        // build an address from the bank info
                        GlobalMemoryLocation loc;
                        memory_region->GetGlobalMemoryLocation(0, &loc);
                        wnd->GoToAddress(loc);
                    }
                }
                ImGui::SameLine();
            }

            ImGui::Text("%s", memory_region->GetName().c_str());

            ImGui::TableNextColumn();
            ImGui::Text("$%04X", memory_region->GetBaseAddress());

            ImGui::TableNextColumn();
            ImGui::Text("$%04X", memory_region->GetRegionSize());
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar(2);
}

} //namespace Windows::NES

