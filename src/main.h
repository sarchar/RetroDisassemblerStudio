#pragma once

#include <memory>
#include <string>
#include <vector>

#include "application.h"
#include "base_window.h"

class MyApp : public Application {
public:
    MyApp(int argc, char* argv[]);
    virtual ~MyApp();

    void AddWindow(BaseWindow* window);

protected:
    virtual bool Update(double deltaTime) override;
    virtual void RenderGUI() override;
    virtual void RenderMainMenuBar() override;
    virtual void RenderMainStatusBar() override;
    virtual bool OnWindowCreated() override;
    virtual void OnKeyPress(int glfw_key, int scancode, int action, int mods) override;

private:

    void CreateROMLoader(std::string const&);

    void OpenROMInfosPane();

    bool request_exit;
    bool show_imgui_demo;

    std::string layout_file;

    // ImGui fonts
    void* main_font;
    void* main_font_bold;

    // Managed child windows
    std::vector<std::unique_ptr<BaseWindow>> managed_windows;
};
