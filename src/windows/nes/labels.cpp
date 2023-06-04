
#include <iomanip>
#include <iostream>
#include <sstream>

#include "imgui.h"
#include "imgui_internal.h"

#include "util.h"

#include "systems/nes/label.h"
#include "systems/nes/memory.h"
#include "systems/nes/system.h"

#include "windows/nes/emulator.h"
#include "windows/nes/listing.h"
#include "windows/nes/labels.h"
#include "windows/nes/project.h"
#include "windows/nes/references.h"

using namespace std;

namespace Windows::NES {

REGISTER_WINDOW(Labels);

shared_ptr<Labels> Labels::CreateWindow()
{
    return make_shared<Labels>();
}

Labels::Labels()
    : BaseWindow(), selected_row(-1), force_resort(true), force_reiterate(true), case_sensitive_sort(false), show_locals(false)
{
    SetTitle("Labels");
    SetNoScrollbar(true);
    
    // create internal signals

    if(auto system = GetSystem()) {
        // grab a weak_ptr so we don't have to continually use dynamic_pointer_cast
        current_system = system;

        // watch for label changes
        label_created_connection = 
            system->label_created->connect(std::bind(&Labels::LabelCreated, this, placeholders::_1, placeholders::_2));
        label_deleted_connection = 
            system->label_deleted->connect(std::bind(&Labels::LabelDeleted, this, placeholders::_1, placeholders::_2));
    }
}

Labels::~Labels()
{
}

void Labels::Update(double deltaTime) 
{
}

void Labels::Render() 
{
    // All access goes through the system
    auto system = current_system.lock();
    if(!system) return;

    {
        bool need_pop = false;
        if(case_sensitive_sort) {
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(255, 0, 0));
            need_pop = true;
        }

        if(ImGui::SmallButton("I")) {
            case_sensitive_sort = !case_sensitive_sort;
            force_resort = true;
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Case Sensitive Sort");
        if(need_pop) ImGui::PopStyleColor(1);

        need_pop = false;
        if(show_locals) {
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(255, 0, 0));
            need_pop = true;
        }

        ImGui::SameLine();
        if(ImGui::SmallButton("L")) {
            show_locals = !show_locals;
            force_reiterate = true;
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Show Local Labels");
        if(need_pop) ImGui::PopStyleColor(1);

        ImGui::Separator();
    }

    // don't render while disassembling
    if(system->IsDisassembling()) return;

    // rebuild the labels list
    if(force_reiterate) {
        labels.clear();
        auto cb = [this](shared_ptr<Label>& label)->void {
            if(!show_locals && label->GetString()[0] == '.') return;
            labels.push_back(label);
        };
        system->IterateLabels(cb);
        force_reiterate = false;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    static ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuterH 
            | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody
            | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Sortable
            | ImGuiTableFlags_ScrollY;

    if(ImGui::BeginTable("LabelsTable", 3, flags)) {
        ImGui::TableSetupColumn("Name"    , ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 0.0f, 0);
        ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthStretch                                    , 0.0f, 1);
        ImGui::TableSetupColumn("RRefs"   , ImGuiTableColumnFlags_WidthFixed   | ImGuiTableColumnFlags_NoSort     , 0.0f, 2);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Sort our data if sort specs have been changed!
        if(ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
            if(force_resort || sort_specs->SpecsDirty) {
                sort(labels.begin(), labels.end(), [&](weak_ptr<Label>& a, weak_ptr<Label>& b)->bool {
                    const ImGuiTableColumnSortSpecs* spec = &sort_specs->Specs[0];
                    bool diff;
                    if(auto bp = b.lock()) {
                        string bstr = bp->GetString();
                        if(!case_sensitive_sort) bstr = strlower(bstr);
                        int bloc = system->GetSortableMemoryLocation(bp->GetMemoryLocation());
                        if(auto ap = a.lock()) {
                            string astr = ap->GetString();
                            if(!case_sensitive_sort) astr = strlower(astr);
                            int aloc = system->GetSortableMemoryLocation(ap->GetMemoryLocation());
                            if(spec->ColumnUserID == 0) {
                                diff = std::tie(astr, aloc) <= std::tie(bstr, bloc); 
                            } else {
                                diff = std::tie(aloc, astr) <= std::tie(bloc, bstr); 
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
        u32 total_labels = labels.size(); //memory_region->GetTotalListingItems();
        //cout << "total_labels = 0x" << hex << total_labels << endl;
        clipper.Begin(total_labels);

        while(clipper.Step()) {
            auto it = labels.begin() + clipper.DisplayStart;
            for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row, ++it) {
                auto label = (*it).lock();
                while(!label && it != labels.end()) {
                    labels.erase(it);
                    label = (*it).lock();
                }

                if(it == labels.end()) break;

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
                        if(ImGui::IsMouseClicked(0)) {
                            if(auto listing = GetMyListing()) {
                                // build an address from the bank info
                                listing->GoToAddress(label->GetMemoryLocation());
                            }
                        } else if(ImGui::IsMouseClicked(1)) {
                            context_row = row;
                            ImGui::OpenPopup("label_context_menu");
                        }
                    }
                    ImGui::SameLine();
                }

                ImGui::Text("%s", label->GetString().c_str());

                ImGui::TableNextColumn();
                stringstream ss;
                ss << "$" << hex << uppercase << setfill('0');

                GlobalMemoryLocation const& loc = label->GetMemoryLocation();
                if(system->CanBank(loc)) {
                    ss << setw(2);
                    if(loc.is_chr) {
                        ss << setw(2) << loc.chr_rom_bank;
                    } else {
                        ss << setw(2) << loc.prg_rom_bank;
                    }
                    ss << ":";
                }
                ss << setw(4) << loc.address;
                ImGui::Text("%s", ss.str().c_str());

                // RRefs
                ImGui::TableNextColumn();
                ImGui::Text("%d", label->GetNumReverseReferences());
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar(2);

    if(ImGui::BeginPopupContextItem("label_context_menu")) {
        if(ImGui::MenuItem("View References")) {
            auto wnd = References::CreateWindow(labels[context_row].lock());
            wnd->SetInitialDock(BaseWindow::DOCK_TOPRIGHT);
            GetMySystemInstance()->AddChildWindow(wnd);
        } else if(ImGui::BeginMenu("Set Breakpoint")) {
            auto bpi = make_shared<BreakpointInfo>();
            bpi->address = labels[context_row].lock()->GetMemoryLocation();
            bpi->has_bank = true;
            bpi->enabled = true;
            if(ImGui::MenuItem("On Execute")) {
                bpi->break_execute = true;
                GetMySystemInstance()->SetBreakpoint(bpi->address, bpi);
            } else if(ImGui::MenuItem("On Read/Write")) {
                bpi->break_read = true;
                bpi->break_write = true;
                GetMySystemInstance()->SetBreakpoint(bpi->address, bpi);
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

}

void Labels::CheckInput()
{
    auto system = current_system.lock();
    if(!system) return;

    if(ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if(selected_row >= 0 && selected_row < labels.size()) {
            if(auto label = labels[selected_row].lock()) {
                system->DeleteLabel(label); // will trigger the LabelDeleted() signal
            }
        }
    }
}

void Labels::LabelCreated(shared_ptr<Label> const& label, bool was_user_created)
{
    if(show_locals || label->GetString()[0] != '.') {
        labels.push_back(label);
        force_resort = true;
    }
}

void Labels::LabelDeleted(shared_ptr<Label> const& label, int nth)
{
    if(show_locals || label->GetString()[0] != '.') {
        auto it = find_if(labels.begin(), labels.end(), [&label](weak_ptr<Label> const& a) {
            if(auto ap = a.lock()) {
                return ap->GetString() == label->GetString();
            }
            return false;
        });
        labels.erase(it);
    }
}

} //namespace Windows::NES

