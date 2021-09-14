#pragma once

#include <memory>
#include <string>
#include <vector>

#include "application.h"
#include "signals.h"
#include "systems/system.h"
#include "windows/base_window.h"

class System;

class MyApp : public Application {
public:
    static MyApp* Instance(int argc = 0, char** argv = nullptr) {
        static MyApp* instance = nullptr;
        if(instance == nullptr) {
            instance = new MyApp(argc, argv);
        }
        return instance;
    }

    // singleton helper
    MyApp(MyApp const& other) = delete;
    void operator=(MyApp const&) = delete;
    virtual ~MyApp();

    void AddWindow(std::shared_ptr<BaseWindow>);

    // Helper dialog boxes
    bool OKPopup(std::string const& title, std::string const& message);

    // Signals
    typedef signal<std::function<void()>> current_system_changed_t;
    std::shared_ptr<current_system_changed_t> current_system_changed;

    // System related
    std::shared_ptr<System> GetCurrentSystem() { return current_system; }

protected:
    MyApp(int argc, char* argv[]);


    virtual bool Update(double deltaTime) override;
    virtual void RenderGUI() override;
    virtual void RenderMainMenuBar() override;
    virtual void RenderMainStatusBar() override;
    virtual bool OnWindowCreated() override;
    virtual void OnKeyPress(int glfw_key, int scancode, int action, int mods) override;

private:
    void ProcessQueuedWindowsForDelete();
    void ManagedWindowClosedHandler(std::shared_ptr<BaseWindow>);

    void CreateROMLoader(std::string const&);
    void SystemLoadedHandler(std::shared_ptr<BaseWindow>, std::shared_ptr<System>);

    void OpenROMInfosPane();

    bool request_exit;
    bool show_imgui_demo;

    std::string layout_file;

    // ImGui fonts
    void* main_font;
    void* main_font_bold;

    // Managed child windows
    std::vector<std::shared_ptr<BaseWindow>> managed_windows;
    std::vector<std::shared_ptr<BaseWindow>> queued_windows_for_delete;

private:
    std::shared_ptr<System> current_system;
};
