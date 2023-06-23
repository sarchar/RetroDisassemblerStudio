// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "signals.h"
#include "windows/basewindow.h"

namespace Systems::NES {
    class Enum;
    class EnumElement;
}

namespace Windows::NES {

class Enums : public BaseWindow {
public:
    using Enum        = Systems::NES::Enum;
    using EnumElement = Systems::NES::EnumElement;

    Enums(bool _select_enum);
    virtual ~Enums();

    virtual char const * const GetWindowClass() { return Enums::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "Windows::NES::Enums"; }
    static std::shared_ptr<Enums> CreateWindow();
    static std::shared_ptr<Enums> CreateWindow(bool select_enum);

    // select enum signals
    make_signal(enum_selected, void(std::shared_ptr<Enum>));

protected:
    void CheckInput() override;
    void Update(double deltaTime) override;
    void Render() override;

    bool SaveWindow(std::ostream&, std::string&) override;
    bool LoadWindow(std::istream&, std::string&) override;

private:
    // selecting vs editing
    bool select_enum;
    bool select_enum_first_focus = true;
    void RenderEnumTable();
    void RenderSelectEnum();

    // sort
    void Resort();
    bool need_resort = false;
    int  sort_column = -1;
    int  selected_row = -1;
    bool reverse_sort = false;
    bool group_by_enum = true;
    bool value_view = false;

    // selection
	std::variant<std::monostate, std::shared_ptr<Enum>, std::shared_ptr<EnumElement>> selected_item;
    template <class T>
    bool IsSelectedItem(T const& t) {
        auto tp = std::get_if<T>(&selected_item);
        return tp && (*tp) == t;
    }
    void RenderContextMenu();
    void DeleteSelectedItem();

    // edit
    void RenderCreateNewEnumRow();
    void RenderCreateNewEnumElementRow(std::shared_ptr<Enum> const&);
    bool creating_new_enum = false;
    bool creating_new_enum_element = false;
    bool editing_name = false;
    bool editing_expression = false;
    bool started_editing = false;
    std::string edit_buffer;
    std::shared_ptr<Enum> edit_enum;
    std::shared_ptr<EnumElement> edit_enum_element;

    // error dialog
    bool wait_dialog = false;
    std::string wait_dialog_message;


    // our list of enums can be grouped by enum or just a list of all the elements
    void RenderEnumRows(bool* = nullptr);
    void RenderEnumElement(std::shared_ptr<EnumElement> const&, bool);
    std::vector<std::shared_ptr<Enum>> enums;
    std::vector<std::shared_ptr<EnumElement>> all_enum_elements;
    std::unordered_map<std::shared_ptr<Enum>, std::vector<std::shared_ptr<EnumElement>>> enum_elements;
};

}
