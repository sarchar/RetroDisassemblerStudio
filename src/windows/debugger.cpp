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
#include "windows/debugger.h"

using namespace std;
#include <stdio.h>

BASE_WINDOW_FUNCTIONAL_CODE_IMPL(SNESDebugger);

shared_ptr<SNESDebugger> SNESDebugger::CreateWindow()
{
    return make_shared<SNESDebugger>();
}

SNESDebugger::SNESDebugger()
    : BaseWindow("snes_debugger")
{
    UpdateTitle();

    // listen for system changes
    current_system_changed_connection = \
            MyApp::Instance()->current_system_changed->connect(std::bind(&SNESDebugger::UpdateTitle, this));
}

SNESDebugger::~SNESDebugger()
{
}

void SNESDebugger::UpdateTitle()
{
    shared_ptr<SNESSystem> system = dynamic_pointer_cast<SNESSystem>(MyApp::Instance()->GetCurrentSystem());
    if(!system) {
        SetTitle("SNES Debugger");
        return;
    }

    stringstream ss;
    ss << "SNES Debugger :: " << system->GetROMFilePathName();
    SetTitle(ss.str());
}

void SNESDebugger::UpdateContent(double deltaTime) 
{
}

void SNESDebugger::RenderContent() 
{
    shared_ptr<SNESSystem> system = dynamic_pointer_cast<SNESSystem>(MyApp::Instance()->GetCurrentSystem());
    stringstream ss;

    if(!system) {
        ImGui::Text("System not loaded");
        return;
    }

    if(ImGui::CollapsingHeader("CPU registers", ImGuiTreeNodeFlags_DefaultOpen)) {
#   define INSPECT_REG(__var, __label, __function, __width) \
            ss.str(""); ss.clear(); \
            __var = __function(); \
            ss << __label << uppercase << setfill('0') << setw(__width) << hex << (unsigned int)__var; \
            ImGui::Text(ss.str().c_str());

        bool v_bool;
        u8   v_u8;
        u16  v_u16;

        INSPECT_REG(v_bool, "E=", system->GetE, 1);

        // v_u8 contains E flag, which forces some values to 1
        bool emu_mode = v_bool;

        ss.str(""); ss.clear();
        v_u8 = system->GetFlags();
        ss << "FLAGS=";

        if(v_u8 & CPU_FLAG_N) ss << "N"; else ss << "n";
        if(v_u8 & CPU_FLAG_V) ss << "V"; else ss << "v";
        if(emu_mode) ss << "1"; else if(v_u8 & CPU_FLAG_M) ss << "M"; else ss << "m";
        if(emu_mode) { if (v_u8 & CPU_FLAG_B) ss << "B"; else ss << "b"; } else { if(v_u8 & CPU_FLAG_X) ss << "X"; else ss << "x"; }
        if(v_u8 & CPU_FLAG_D) ss << "D"; else ss << "d";
        if(v_u8 & CPU_FLAG_I) ss << "I"; else ss << "i";
        if(v_u8 & CPU_FLAG_Z) ss << "Z"; else ss << "z";
        if(v_u8 & CPU_FLAG_C) ss << "C"; else ss << "c";
        ImGui::SameLine();
        ImGui::Text(ss.str().c_str());

        INSPECT_REG(v_u16, "PC=$", system->GetPC, 4);
        ImGui::SameLine();

        if(emu_mode) {
            INSPECT_REG(v_u8, "A=$", system->GetA, 2);
            ImGui::SameLine();
            INSPECT_REG(v_u8, "X=$", system->GetXL, 2);
            ImGui::SameLine();
            INSPECT_REG(v_u8, "Y=$", system->GetYL, 2);
        } else {
        }

        // in both emu and native mode, but fixed values in emu mode
        INSPECT_REG(v_u8, "PBR=$", system->GetPBR, 2);
        ImGui::SameLine();
        INSPECT_REG(v_u8, "DBR=$", system->GetDBR, 2);
        ImGui::SameLine();
        INSPECT_REG(v_u16, "D=$", system->GetD, 4);
        ImGui::SameLine();
        INSPECT_REG(v_u16, "S=$", system->GetS, 4);

#   undef INSPECT_REG
    }

    if(ImGui::CollapsingHeader("System signals", ImGuiTreeNodeFlags_DefaultOpen)) {
#   define INSPECT_SIGNAL(__var, __label, __function, __width, __high_z_str) \
            ss.str(""); ss.clear(); \
            __var = __function(); \
            ss << __label; \
            if(__var.has_value()) ss << uppercase << setfill('0') << setw(__width) << hex << (unsigned int)*__var; \
            else                  ss << __high_z_str; \
            ImGui::Text(ss.str().c_str());
            
        std::optional<bool> v_bool;
        std::optional<u8> v_u8;
        std::optional<u16> v_u16;
        std::optional<u32> v_u32;

        INSPECT_SIGNAL(v_bool, "RWn=", system->GetSignalRWn, 1, "z");
        ImGui::SameLine();
        INSPECT_SIGNAL(v_bool, "VPA=", system->GetSignalVPA, 1, "z");
        ImGui::SameLine();
        INSPECT_SIGNAL(v_bool, "VDA=", system->GetSignalVDA, 1, "z");
        ImGui::SameLine();
        INSPECT_SIGNAL(v_bool, "VPn=", system->GetSignalVPn, 1, "z");
        ImGui::SameLine();
        INSPECT_SIGNAL(v_bool, "E=", system->GetSignalE, 1, "z");
        ImGui::SameLine();
        INSPECT_SIGNAL(v_bool, "MX=", system->GetSignalMX, 1, "z");

        INSPECT_SIGNAL(v_u8, "DB=$", system->GetSignalDB, 2, "zz");
        ImGui::SameLine();
        INSPECT_SIGNAL(v_u16, "A=$", system->GetSignalA, 4, "zzzz");

        ImGui::Separator();
        ImGui::Text("SNESAddressDecoder");
        INSPECT_SIGNAL(v_u8, "D=$", system->GetADSignalD, 2, "zz");
        ImGui::SameLine();
        INSPECT_SIGNAL(v_u32, "A=$", system->GetADSignalA, 6, "zzzzzz");
        ImGui::SameLine();
        INSPECT_SIGNAL(v_bool, "RAM_CSn=", system->GetADSignalRAMCSn, 1, "z");
        ImGui::SameLine();
        INSPECT_SIGNAL(v_bool, "ROM_CSn=", system->GetADSignalROMCSn, 1, "z");
#   undef INSPECT_SIGNAL
    }

    if(ImGui::CollapsingHeader("Control", ImGuiTreeNodeFlags_DefaultOpen)) {
        if(ImGui::Button("Step System Clock")) {
            system->IssueStepSystem();
        }

        ImGui::SameLine();
        if(ImGui::Button("Step CPU")) {
            system->IssueStepCPU();
        }

        ImGui::SameLine();
        if(system->IsRunning()) {
            if(ImGui::Button("Stop")) {
                system->IssueStop();
            }
        } else {
            if(ImGui::Button("Run")) {
                system->IssueRun();
            }
        }

        if(ImGui::Button("Reset")) {
            system->IssueReset();
        }
    }

}
