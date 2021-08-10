#pragma once

#include <string>

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#undef CreateWindow

class Application {
public:
    Application(std::string const& _window_title, int _window_width, int _window_height);
    virtual ~Application();

    int Run();

    virtual bool Update(double deltaTime);
    virtual void RenderGL();
    virtual void RenderGUI();
    virtual void RenderMainMenuBar();
    virtual void RenderMainStatusBar();
    virtual void RenderMainToolBar();

    virtual void OnKeyPress(int glfw_key, int scancode, int action, int mods);

    virtual bool OnWindowCreated();
    virtual void OnWindowDestroyed();

    void SetEnableStatusBar(bool enabled) { enable_statusbar = enabled; }
    void SetEnableToolBar(bool enabled) { enable_toolbar = enabled; }
    void SetWindowPos(int x, int y);

protected:
    ImVec4 clear_color;

    void ShowDockSpace(bool dockSpaceHasBackground = true);

private:
    int CreateWindow();
    void DestroyWindow();
    void KeyPressHandler(GLFWwindow*, int, int, int, int);
    void _RenderMainStatusBar();
    void _RenderMainToolBar();

    GLFWwindow* glfw_window;
    int         window_width;
    int         window_height;
    std::string window_title;
    bool        enable_statusbar;
    bool        enable_toolbar;

};
