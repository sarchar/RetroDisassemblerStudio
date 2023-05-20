#pragma once

#include <string>

#include "windows/baseproject.h"

namespace NES {

struct CreateNewDefineData {};

class Project : public BaseProject {
public:
    virtual char const * const GetWindowClass() { return Project::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::Project"; }

    BaseProject::Information const* GetInformation();

    Project();
    virtual ~Project();

    bool CreateNewProjectFromFile(std::string const&) override;

    void CreateDefaultWorkspace() override;

    // creation interface
    static BaseProject::Information const* GetInformationStatic();
    static bool IsROMValid(std::string const&, std::istream&);
    static std::shared_ptr<BaseProject> CreateProject();

    // Save and Load
    bool Save(std::ostream&, std::string&) override;
    bool Load(std::istream&, std::string&) override;

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;

private:
    void WindowAdded(std::shared_ptr<BaseWindow>&) override;

    void CommonCommandHandler(std::shared_ptr<BaseWindow>&, std::string const&, void*);

    bool StartPopup(std::string const&, bool = true);
    int EndPopup(int, bool show_ok = true, bool show_cancel = true, bool allow_escape = true);

    void RenderPopups();
    void RenderCreateNewDefinePopup();

    struct {
        struct {
            bool        show = false;
            std::string title;
            std::string content;
        } ok;

        struct {
            bool        show = false;
            std::string title = "Create New Define";
            bool        focus;
        } create_new_define;

        // Temp editing buffers for various dialogs
        std::string buffer1;
        std::string buffer2;
        int selected_index;

        std::string current_title = "";
    } popups;
};

}
