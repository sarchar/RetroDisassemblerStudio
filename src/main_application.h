#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "application.h"
#include "signals.h"

namespace Windows {
    class BaseWindow;
}

class BaseProject;
class WindowRegistration;

#define GetApplication() MainApplication::Instance()

class MainApplication : public Application {
public:
    static MainApplication* Instance(int argc = 0, char** argv = nullptr) {
        static MainApplication* instance = nullptr;
        if(instance == nullptr) {
            instance = new MainApplication(argc, argv);
        }
        return instance;
    }

    // singleton helper
    MainApplication(MainApplication const& other) = delete;
    void operator=(MainApplication const&) = delete;
    virtual ~MainApplication();

    // Windows
    std::shared_ptr<Windows::BaseWindow> CreateMainWindow() override;
    std::shared_ptr<Windows::BaseWindow> CreateWindow(std::string const&);
 
    void* GetBoldFont() { return main_font_bold; }

    // Signals

protected:
    MainApplication(int argc, char* argv[]);

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

    std::unordered_map<std::string, WindowRegistration*> window_classes;
};

class WindowRegistration
{
public:
    typedef std::shared_ptr<Windows::BaseWindow> (CreateWindowFunc)();
    WindowRegistration(std::string const& _window_class, CreateWindowFunc* _create_func) 
        : window_class(_window_class), create_func(_create_func) {
        static WindowRegistration* _head = nullptr; // guaranteed to be initialized before anything else

        next = _head;
        _head = this;

        head = _head;
    }

private:
    WindowRegistration* next;
    static WindowRegistration* head;

    std::string window_class;
    CreateWindowFunc* create_func;

    friend class MainApplication;
};

#define REGISTER_WINDOW(cls) \
    namespace __rw_##cls {     \
        static std::shared_ptr<BaseWindow> _create_##cls() { \
            return cls::CreateWindow();                      \
        }                                                    \
        static WindowRegistration \
            window_registration(cls::GetWindowClassStatic(), &_create_##cls); \
    }

