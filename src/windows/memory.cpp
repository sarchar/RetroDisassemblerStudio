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

using namespace std;
#include <stdio.h>

shared_ptr<SNESMemory> SNESMemory::CreateWindow()
{
    return make_shared<SNESMemory>();
}

SNESMemory::SNESMemory()
    : BaseWindow("SNES Memory")
{
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

    const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

    static ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersOuter;
    static char const * const columns[] = { "", "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0A", "0B", "0C", "0D", "0E", "0F" };

    if (ImGui::BeginTable("snes_memory", 17, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        for(int col = 0; col < 17; col++) {
            ImGui::TableSetupColumn(columns[col], ImGuiTableColumnFlags_None);
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
}
