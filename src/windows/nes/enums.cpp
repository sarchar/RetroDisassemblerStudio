#include <iostream>
#include <string>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "systems/nes/enum.h"
#include "systems/nes/expressions.h"
#include "systems/nes/system.h"

#include "windows/main.h"
#include "windows/nes/emulator.h"
#include "windows/nes/enums.h"
#include "windows/nes/listing.h"
#include "windows/nes/project.h"
#include "windows/nes/references.h"

using namespace std;

namespace Windows::NES {

REGISTER_WINDOW(Enums);

std::shared_ptr<Enums> Enums::CreateWindow()
{
    return CreateWindow(false);
}

std::shared_ptr<Enums> Enums::CreateWindow(bool select_enum)
{
    return make_shared<Enums>(select_enum);
}

Enums::Enums(bool _select_enum)
    : select_enum(_select_enum)
{
    SetNav(false);

    if(select_enum) {
        SetDockable(false);
        SetPopup(true);
        SetNoScrollbar(true);
        SetTitle("Select Enum");
    } else {
        SetTitle("Enums");
    }

    // refresh enums if they change in the system
    auto system = GetSystem();
    *system->enum_created         += [this](shared_ptr<Enum> const&)             { need_resort = true; };
    *system->enum_deleted         += [this](shared_ptr<Enum> const&)             { need_resort = true; };
    *system->enum_element_added   += [this](shared_ptr<EnumElement> const&)      { need_resort = true; };
    *system->enum_element_changed += [this](shared_ptr<EnumElement> const&, s64) { need_resort = true; };
    *system->enum_element_deleted += [this](shared_ptr<EnumElement> const&)      { need_resort = true; };
}

Enums::~Enums()
{
}

void Enums::CheckInput()
{
    if(select_enum) {
        if(ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ClosePopup();
        }
    } else {
        if(ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            DeleteSelectedItem();
        }
    }
}

void Enums::Update(double deltaTime)
{
    if(need_resort) {
        Resort();
        need_resort = false;
    }
}

void Enums::Resort()
{
    enums.clear();
    enum_elements.clear();
    all_enum_elements.clear();

    GetSystem()->IterateEnums([this](shared_ptr<Enum> const& e) {
        if(group_by_enum) {
            enums.push_back(e);
        }

        e->IterateElements([this, &e](shared_ptr<EnumElement> const& ee) {
            if(group_by_enum) {
                enum_elements[e].push_back(ee);
            } else {
                all_enum_elements.push_back(ee);
            }
        });

        // no sorting?
        if(sort_column == -1) return;

        // sort the elements within the group
        auto sort_func = 
            [this](shared_ptr<EnumElement> const& ee_a, shared_ptr<EnumElement> const& ee_b)->bool {
                bool diff = false;

                if(sort_column == 0) {
                    if(reverse_sort) diff = ee_b->GetName() <= ee_a->GetName();
                    else             diff = ee_a->GetName() <= ee_b->GetName();
                } else if(sort_column == 1) {
                    if(reverse_sort) diff = ee_b->cached_value <= ee_a->cached_value;
                    else             diff = ee_a->cached_value <= ee_b->cached_value;
                } else if(sort_column == 2) {
                    if(reverse_sort) diff = ee_b->GetNumReverseReferences() <= ee_a->GetNumReverseReferences();
                    else             diff = ee_a->GetNumReverseReferences() <= ee_b->GetNumReverseReferences();
                }

                return diff;
            };

        if(group_by_enum) {
            sort(enum_elements[e].begin(), enum_elements[e].end(), sort_func);
        } else {
            sort(all_enum_elements.begin(), all_enum_elements.end(), sort_func);
        }
    });

    if(sort_column == -1 || !group_by_enum) return; // no sorting (sort by order added)

    // enum names are only sorted by by their name
    sort(enums.begin(), enums.end(), 
        [this](shared_ptr<Enum> const& a, shared_ptr<Enum> const& b)->bool {
            bool diff;

            if(reverse_sort) diff = b->GetName() <= a->GetName();
            else             diff = a->GetName() <= b->GetName();

            return diff;
        }
    );
}

void Enums::Render()
{
    if(select_enum) RenderSelectEnum();
    else            RenderEnumTable();
}

void Enums::RenderSelectEnum()
{
    ImGui::PushItemWidth(-FLT_MIN);
    bool enter_pressed = ImGui::InputText("##edit_name", &edit_buffer, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SetItemDefaultFocus();
    if(select_enum_first_focus) {
        ImGui::SetKeyboardFocusHere(-1);
        if(ImGui::IsItemActive()) select_enum_first_focus = false;
    }

    ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchSame
        | ImGuiTableFlags_Sortable | ImGuiTableFlags_NoHostExtendY;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(-1, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(-1, 0));

    auto outer_size = ImVec2(-1, ImGui::GetTextLineHeight() * 14);
    if(ImGui::BeginTable("select_enum_table", 1, table_flags, outer_size)) {
        ImGui::TableSetupColumn("Name" , ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);

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

        RenderEnumRows(&enter_pressed);

        ImGui::EndTable();
    }
    
    ImGui::PopStyleVar(2);

    if(ImGui::Button("OK") || enter_pressed) {
        if(selected_item.index() != 0) {
            enum_selected->emit(get<shared_ptr<Enum>>(selected_item));
            ClosePopup();
        }
    }

    ImGui::SameLine();
    if(ImGui::Button("Cancel")) {
        ClosePopup();
    }
}

void Enums::RenderEnumTable()
{
    if(wait_dialog) {
        if(GetMainWindow()->OKPopup("Enum error", wait_dialog_message)) {
            wait_dialog = false;
            started_editing = true; // re-edit the expression
        }
    }

    if(ImGuiFlagButton(&group_by_enum, "G", "Group by parent Enum")) {
        need_resort = true;
    }

    ImGui::SameLine(); ImGuiFlagButton(&value_view, "V", "Toggle expression/value view");

    if(group_by_enum) {
        ImGui::SameLine();
        if(ImGuiFlagButton(nullptr, "+", "Create new Enum")) {
            creating_new_enum = true;
            started_editing = true;
            edit_buffer = "";
        }
    }

    ImGui::Separator();

    ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoBordersInBodyUntilResize
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchSame
        | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(-1, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(-1, 0));

    if(ImGui::BeginTable("enums_table", 3, table_flags)) {
        ImGui::TableSetupColumn("Name" , ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
        ImGui::TableSetupColumn(value_view ? "Value##Value" : "Expression##Value", 
                                         ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);
        ImGui::TableSetupColumn("RRefs", ImGuiTableColumnFlags_WidthStretch, 0.0f, 2);
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

        if(group_by_enum) {
            RenderEnumRows();
            RenderCreateNewEnumRow();
        } else {
            for(auto& ee: all_enum_elements) {
                RenderEnumElement(ee, true);
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar(2);

    RenderContextMenu();
}

void Enums::RenderEnumRows(bool* double_clicked)
{
    shared_ptr<Enum> to_select_enum;

    for(auto& e : enums) {
        // filter the list based on edit_buffer
        if(select_enum && edit_buffer.size()) {
            if(e->GetName().find(edit_buffer) != 0) continue;
        }

        ImGui::TableNextRow();
        
        // default to selecting the first visible enum for the select_enum popup
        // but keep the currently selected enum
        if(!to_select_enum || IsSelectedItem(e)) to_select_enum = e;

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        // render the selectable
        ImGui::TableNextColumn();
        {
            stringstream ss;
            ss << "##" << e.get();
            if(ImGui::Selectable(ss.str().c_str(), IsSelectedItem(e), ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                selected_item = e;
                to_select_enum = e;
	    	}

            if(ImGui::IsItemHovered()) {
                if(ImGui::IsMouseClicked(1)) {
                    selected_item = e;
                    to_select_enum = e;
                    ImGui::OpenPopup("enum_context_menu");
                } else if(double_clicked && ImGui::IsMouseDoubleClicked(0)) {
                    *double_clicked = true;
                }
            }
            ImGui::SameLine();
        }

        stringstream ss;
        ss << "enum " << e->GetName();
        if(select_enum) {
            ImGui::Text(ss.str().c_str());
        } else {
            auto open = ImGui::TreeNodeEx(ss.str().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%d elements", enum_elements[e].size());

            ImGui::TableNextColumn();
            ImGui::Text("%d", e->GetNumReverseReferences());
            if(!open) continue;

            for(auto& ee : enum_elements[e]) RenderEnumElement(ee, false);

            RenderCreateNewEnumElementRow(e);

            ImGui::TreePop();
        }
    }

    // automatically select an enum in the popup
    if(select_enum) selected_item = to_select_enum;
}

void Enums::RenderEnumElement(std::shared_ptr<EnumElement> const& ee, bool show_formatted_name)
{
    ImGui::TableNextRow();

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // render the selectable
    ImGui::TableNextColumn();
    {
        stringstream ss;
        ss << "##" << ee.get();
        if(ImGui::Selectable(ss.str().c_str(), IsSelectedItem(ee), ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
            selected_item = ee;
		}
        
        if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) {
            selected_item = ee;
            ImGui::OpenPopup("enum_context_menu");
        }
        ImGui::SameLine();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // render the name column
    if(!(editing_name && ee == edit_enum_element)) {
        ImGui::Text("%s", (show_formatted_name ? ee->GetFormattedName("_") : ee->GetName()).c_str());
        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            edit_buffer = ee->GetName();
            edit_enum_element = ee;
            editing_name = true;
            started_editing = true;
        }
    } else {
        ImGui::PushItemWidth(-FLT_MIN);
        bool enter_pressed = ImGui::InputText("##edit_name", &edit_buffer, ImGuiInputTextFlags_EnterReturnsTrue);

        // if wait_dialog is open, don't go any further
        if(!wait_dialog) {
            if(started_editing) {
                ImGui::SetKeyboardFocusHere(-1);

                // wait until item is activated
                if(ImGui::IsItemActive()) started_editing = false;
            } else if(!ImGui::IsItemActive()) { // check if item lost activation. stop editing without saving
                editing_name = false;
            }

            // wait until enter is pressed
            if(enter_pressed) {
                // set the name and tell the enum to trigger changed signal
                if(edit_buffer != ee->GetName()) {
                    auto e = ee->parent_enum.lock();
                    string errmsg;
                    auto ret = e->ChangeElementName(ee, edit_buffer, errmsg);
                    assert(ret);
                }

                editing_name = false;
                need_resort = true;
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // render the expression column
    ImGui::TableNextColumn();
    if(!(editing_expression && ee == edit_enum_element)) { // not editing
        if(value_view) {
            ImGui::Text("$%X", ee->cached_value);
            if(ImGui::IsItemHovered()) {
                stringstream ss;
                ss << *ee->GetExpression();
                ImGui::SetTooltip("%s", ss.str().c_str());
            }
        } else {
            stringstream ss;
            ss << *ee->GetExpression();
            ImGui::Text("%s", ss.str().c_str());
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("$%X", ee->cached_value);
            }
        }

        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            stringstream ss;
            ss << *ee->GetExpression();
            edit_buffer = ss.str();
            edit_enum_element = ee;
            editing_expression = true;
            started_editing = true;
        }
    } else { // editing
        ImGui::PushItemWidth(-FLT_MIN);
        bool enter_pressed = ImGui::InputText("##edit_expression", &edit_buffer, ImGuiInputTextFlags_EnterReturnsTrue);

        // if wait_dialog is open, don't go any further
        if(wait_dialog) goto done_expression;

        if(started_editing) {
            ImGui::SetKeyboardFocusHere(-1);

            // wait until item is activated
            if(ImGui::IsItemActive()) started_editing = false;
        } else if(!ImGui::IsItemActive()) { // check if item lost activation. stop editing without saving
            editing_expression = false;
        }

        // wait until enter is pressed
        if(!enter_pressed) goto done_expression;

        // the expression needs to be evaluatable
        auto expr = make_shared<Systems::NES::Expression>();
        int errloc;
        string errmsg;
        if(!expr->Set(edit_buffer, errmsg, errloc)) {
            stringstream ss;
            ss << "There was a problem parsing the expression: " << errmsg << " (at offset " << errloc << ")" << endl;
            wait_dialog = true;
            wait_dialog_message = ss.str();
            goto done_expression;
        }

        auto e = ee->parent_enum.lock();
        if(e) { // if parent was somehow deleted while we were editing (possible!), fail silently and re-sort
            // don't allow labels, defines, dereferences, etc in enums. they need to evaluate to constants
            // and then update the expression (have to inform the Enum class about value changes)
            if(!GetSystem()->FixupExpression(expr, errmsg, 0)
                || !e->ChangeElementExpression(ee, expr, errmsg)) {
                stringstream ss;
                ss << "There was a problem evaluating the expression: " << errmsg;
                wait_dialog = true;
                wait_dialog_message = ss.str();
                goto done_expression;
            }
        }

        // done
        editing_expression = false;

        // resort to display our new enum
        need_resort = true;
    }

done_expression:
    ImGui::TableNextColumn();
    ImGui::Text("%d", ee->GetNumReverseReferences());
}

void Enums::RenderCreateNewEnumRow()
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    if(!creating_new_enum) {
        ImGui::TextDisabled("<New Enum>");
        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            creating_new_enum = true;
            started_editing = true;
            edit_buffer = "";
        }

        return;
    }

    ImGui::PushItemWidth(-FLT_MIN);
    bool enter_pressed = ImGui::InputText("##new_enum_name", &edit_buffer, ImGuiInputTextFlags_EnterReturnsTrue);

    // if wait_dialog is open, don't go any further
    if(wait_dialog) return;

    if(started_editing) {
        ImGui::SetKeyboardFocusHere(-1);
        // wait until item is activated
        if(ImGui::IsItemActive()) started_editing = false;
    } else if(!ImGui::IsItemActive()) { // check if item lost activation
                                        // stop editing without saving
        creating_new_enum = false;
    }

    // wait until enter is pressed
    if(!enter_pressed) return;

    // to validate the enum name, we can parse an expression and check that the root node is a Name node
    auto expr = make_shared<Systems::NES::Expression>();
    int errloc;
    string errmsg;
    if(!expr->Set(edit_buffer, errmsg, errloc)) {
        wait_dialog = true;
        wait_dialog_message = "Invalid name";
        return;
    }

    auto name_node = dynamic_pointer_cast<BaseExpressionNodes::Name>(expr->GetRoot());
    if(!name_node) {
        wait_dialog = true;
        wait_dialog_message = "Invalid name";
        return;
    }

    edit_buffer = name_node->GetString();

    // create the enum with this name
    if(!GetSystem()->CreateEnum(edit_buffer)) {
        wait_dialog = true;
        wait_dialog_message = "Enum already exists";
        return;
    }

    creating_new_enum = false;

    // resort to display our new enum
    need_resort = true;
}

void Enums::RenderCreateNewEnumElementRow(std::shared_ptr<Enum> const& for_enum)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    if(!creating_new_enum_element) {
        ImGui::TextDisabled("<New Element>");
        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            creating_new_enum_element = true;
            started_editing = true;
            edit_buffer = "";
            edit_enum = for_enum;
        }

        return;
    }

    // if it's not this particular enum, don't render it
    if(for_enum != edit_enum) return;

    ImGui::PushItemWidth(-FLT_MIN);
    bool enter_pressed = ImGui::InputText("##new_enum_element_name", &edit_buffer, ImGuiInputTextFlags_EnterReturnsTrue);

    // if wait_dialog is open, don't go any further
    if(wait_dialog) return;

    if(started_editing) {
        ImGui::SetKeyboardFocusHere(-1);
        // wait until item is activated
        if(ImGui::IsItemActive()) started_editing = false;
    } else if(!ImGui::IsItemActive()) { // check if item lost activation
                                        // stop editing without saving
        creating_new_enum_element = false;
    }

    // wait until enter is pressed
    if(!enter_pressed) return;

    // to validate the enum name, we can parse an expression and check that the root node is a Name node
    {
        auto expr = make_shared<Systems::NES::Expression>();
        int errloc;
        string errmsg;
        if(!expr->Set(edit_buffer, errmsg, errloc)) {
            wait_dialog = true;
            wait_dialog_message = "Invalid name";
            return;
        }

        auto name_node = dynamic_pointer_cast<BaseExpressionNodes::Name>(expr->GetRoot());
        if(!name_node) {
            wait_dialog = true;
            wait_dialog_message = "Invalid name";
            return;
        }

        edit_buffer = name_node->GetString();
    }

    // create a default expression with value 0
    auto expr = Systems::NES::Expression::FromString("0");
    assert(expr);

    // prevent re-sort unless it's already set
    auto save_resort = need_resort;

    // create the element
    auto ee = for_enum->CreateElement(edit_buffer, expr);
    if(!ee) {
        wait_dialog = true;
        wait_dialog_message = "Element name already used";
        return;
    }

    // after we finish creating an element name, move to edit the expression immediately
    creating_new_enum_element = false;
    editing_expression = true;
    started_editing = true;
    edit_enum_element = ee;
    edit_buffer = "0";

    // don't re-sort yet, wait for the expression editing to finish. but we need to add this element to the list
    auto& list = enum_elements[for_enum];
    list.push_back(ee);

    need_resort = save_resort;
}

void Enums::RenderContextMenu()
{
    if(!ImGui::BeginPopupContextItem("watch_context_menu")) return;

    if(selected_item.index() != 0) {
        if(ImGui::MenuItem("View References")) {
            shared_ptr<BaseWindow> wnd;
            if(auto enum_type_ptr = get_if<shared_ptr<Enum>>(&selected_item)) {
                wnd = References::CreateWindow(*enum_type_ptr);
            } else if(auto enum_element_ptr = get_if<shared_ptr<EnumElement>>(&selected_item)) {
                wnd = References::CreateWindow(*enum_element_ptr);
            }

            wnd->SetInitialDock(BaseWindow::DOCK_RIGHTTOP);
            GetMySystemInstance()->AddChildWindow(wnd);
        }
    }

    if(selected_item.index() == 1) {
        auto enum_type = get<shared_ptr<Enum>>(selected_item);

        if(ImGui::BeginMenu("Set Data Size")) {
            if(ImGui::MenuItem("Byte", nullptr, enum_type->GetSize() == 1)) {
                if(enum_type->GetNumReverseReferences()) {
                    wait_dialog = true;
                    wait_dialog_message = "Cannot change enum size while enum is in use";
                } else {
                    enum_type->SetSize(1);
                }
            }

            if(ImGui::MenuItem("Word", nullptr, enum_type->GetSize() == 2)) {
                if(enum_type->GetNumReverseReferences()) {
                    wait_dialog = true;
                    wait_dialog_message = "Cannot change enum size while enum is in use";
                } else {
                    enum_type->SetSize(2);
                }
            }
            ImGui::EndMenu();
        }
    }

    if(selected_item.index() != 0) {
        if(ImGui::MenuItem("Delete")) {
            DeleteSelectedItem();
        }
    }

    ImGui::EndPopup();
}

void Enums::DeleteSelectedItem()
{
    if(selected_item.index() == 0) return;

    if(auto ee = get_if<shared_ptr<EnumElement>>(&selected_item)) {
        if((*ee)->GetNumReverseReferences()) {
            wait_dialog = true;
            wait_dialog_message = "Enum element is in use and cannot be deleted";
        } else {
            auto e = (*ee)->parent_enum.lock();
            if(e) e->DeleteElement(*ee);
        }
    } else if(auto e = get_if<shared_ptr<Enum>>(&selected_item)) {
        bool has_rrefs = false;
        (*e)->IterateElements([&has_rrefs](shared_ptr<EnumElement> const& ee) {
            if(ee->GetNumReverseReferences()) has_rrefs = true;
        });

        if(has_rrefs) {
            wait_dialog = true;
            wait_dialog_message = "One or more elements of the enum are in use and cannot be deleted";
        } else {
            GetSystem()->DeleteEnum(*e);
        }
    }
}

bool Enums::SaveWindow(std::ostream& os, std::string& errmsg)
{
    return true;
}

bool Enums::LoadWindow(std::istream& is, std::string& errmsg)
{
    need_resort = true;
    return true;
}

}
