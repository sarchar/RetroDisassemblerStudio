#pragma once

#include <string>

#undef CreateWindow

class GLFWwindow;

namespace Windows {
    class BaseWindow;
}

class Application {
public:
    using BaseWindow = Windows::BaseWindow;

    virtual ~Application();

    virtual std::shared_ptr<BaseWindow> CreateMainWindow() = 0;
    template<class T>
    std::shared_ptr<T> GetMainWindowAs() {
        return dynamic_pointer_cast<T>(main_window);
    }

    int Run();

    virtual bool Update(double deltaTime);
    virtual void RenderGL();

    virtual void OnKeyPress(int glfw_key, int scancode, int action, int mods);
    virtual void OnWindowMoved(int x, int y);

    virtual bool OnPlatformReady();
    virtual void OnPlatformClosed();

    void SetWindowPos(int x, int y);

    // Autodocking new windows requires these utility functions
    // Do not save the ID return values, as they may change when the dockspace builder is recreated
//!    unsigned int GetDockspaceImGuiID() const { return imgui_dockspace_id; }
//!    unsigned int GetDockBuilderRootID() const { return imgui_dock_builder_root_id; }
//!    unsigned int GetDockBuilderLeftID() const { return imgui_dock_builder_left_id; }
//!    unsigned int GetDockBuilderRightID() const { return imgui_dock_builder_right_id; }
//!    unsigned int GetDockBuilderBottomID() const { return imgui_dock_builder_bottom_id; }
//!
//!
//!    bool has_dock_builder;

protected:
    Application(std::string const& _window_title, int _window_width, int _window_height);

protected:
    float clear_color[4];

private:
    int  CreatePlatformWindow();
    void DestroyPlatformWindow();
    void KeyPressHandler(GLFWwindow*, int, int, int, int);
    void WindowPosHandler(GLFWwindow*, int, int);

    uintptr_t   _glfw_window;
    int         window_width;
    int         window_height;
    std::string window_title;

    std::shared_ptr<BaseWindow> main_window;
};
