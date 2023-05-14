
#include "imgui.h"
#include "imgui_internal.h"

#include "main.h"
#include "windows/nes/listing.h"
#include "windows/nes/regions.h"
#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_system.h"

using namespace std;

namespace NES {

namespace Windows {

shared_ptr<MemoryRegions> MemoryRegions::CreateWindow()
{
    return make_shared<MemoryRegions>();
}

MemoryRegions::MemoryRegions()
    : BaseWindow("NES::MemoryRegions"), selected_row(-1)
{
    SetTitle("Memory Regions");
    
    // create internal signals

    if(auto system = (MyApp::Instance()->GetProject()->GetSystem<System>())) {
        // grab a weak_ptr so we don't have to continually use dynamic_pointer_cast
        current_system = system;
    }
}

MemoryRegions::~MemoryRegions()
{
}

void MemoryRegions::UpdateContent(double deltaTime) 
{
}

void MemoryRegions::RenderContent() 
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
        // The first column will use the default _WidthStretch when ScrollX is Off and _WidthFixed when ScrollX is On
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed);
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

                if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if(auto wnd = MyApp::Instance()->FindMostRecentWindow<Listing>()) {
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

} //namespace Windows
} //namespace NES

