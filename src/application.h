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
    void SetTitle(std::string const&);

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
