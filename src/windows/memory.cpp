// TODO in the future will need a way to make windows only available to the currently loaded system
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

#include "main.h"
#include "signals.h"
#include "systems/snes/snes_system.h"
#include "windows/memory.h"

//#define USE_IMGUI_MEMORY_EDITOR
#ifdef USE_IMGUI_MEMORY_EDITOR
#   include "../../imgui_club/imgui_memory_editor/imgui_memory_editor.h"
#endif

using namespace std;
#include <stdio.h>

shared_ptr<SNESMemory> SNESMemory::CreateWindow()
{
    return make_shared<SNESMemory>();
}

SNESMemory::SNESMemory()
    : BaseWindow("snes_memory")
{
    SetTitle("SNES Memory");
}

SNESMemory::~SNESMemory()
{
}

void SNESMemory::UpdateContent(double deltaTime) 
{
}

void SNESMemory::RenderContent() 
{
    shared_ptr<SNESSystem> system = dynamic_pointer_cast<SNESSystem>(MyApp::Instance()->GetCurrentSystem());

    if(!system) {
        ImGui::Text("System not loaded");
        return;
    }

#ifdef USE_IMGUI_MEMORY_EDITOR
    static MemoryEditor mem_edit;
    static u8 memory[16384];
    mem_edit.DrawContents(memory, 16384);
    return;
#endif

    const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

    static ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedFit;
    static char const * const columns[] = { "", "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0A", "0B", "0C", "0D", "0E", "0F" };

    auto textWidthOne = ImGui::CalcTextSize("0").x;
    ImVec2 cell_padding(textWidthOne, 0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cell_padding);
    if (ImGui::BeginTable("snes_memory", 17, flags))
    {
        // set the column width to exactly the size of the text, which is uniform across the table
        auto textWidthAddr  = ImGui::CalcTextSize("0000").x;
        auto textWidthValue = ImGui::CalcTextSize("00").x;

        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        for(int col = 0; col < 17; col++) {
            ImGui::TableSetupColumn(columns[col], ImGuiTableColumnFlags_None, (col == 0) ? textWidthAddr : textWidthValue);
        }
        ImGui::TableHeadersRow();

        // Demonstrate using clipper for large vertical lists
        ImGuiListClipper clipper;
        clipper.Begin(0x2000 / 16); // 0x2000 bytes divided by 16 bytes per row
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                ImGui::TableNextRow();
                u16 address = row * 16;
                char buf[16];
                sprintf(buf, "%04X", address);
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(buf);

                // nab 16 bytes starting at address
                u8 values[16];
                system->GetRAM(values, address, 16);

                for (int column = 1; column < 17; column++)
                {
                    ImGui::TableSetColumnIndex(column);
                    sprintf(buf, "%02X", values[column - 1]);
                    ImGui::Text(buf);
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}
