#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "application.h"
#include "signals.h"
#include "systems/nes/nes_memory.h"
#include "windows/basewindow.h"

class BaseProject;

#define GetApplication() MyApp::Instance()

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

    // Windows
    std::shared_ptr<Windows::BaseWindow> CreateMainWindow() override;

    void* GetBoldFont() { return main_font_bold; }

    // Signals

protected:
    MyApp(int argc, char* argv[]);

    bool Update(double deltaTime) override;
    bool OnPlatformReady() override;

private:
    bool request_exit;

    std::string layout_file;

    // ImGui fonts
    void* main_font;
    void* main_font_bold;

private:
    typedef std::function<std::shared_ptr<BaseWindow>(void)> create_window_func;
    std::map<std::string, create_window_func> create_window_functions;

private:
    struct WindowFromINI {
        std::string window_class;
        std::string window_id;
    };

    void SetupINIHandlers();
    WindowFromINI* NewINIWindow();
    void CreateINIWindows();
    std::vector<std::shared_ptr<WindowFromINI>> ini_windows;
};
