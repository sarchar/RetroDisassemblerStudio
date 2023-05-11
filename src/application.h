#pragma once

#include <string>

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#include "imgui.h"

#undef CreateWindow

class Application {
public:
    virtual ~Application();

    int Run();

    virtual bool Update(double deltaTime);
    virtual void RenderGL();
    virtual void RenderGUI();
    virtual void RenderMainMenuBar();
    virtual void RenderMainStatusBar();
    virtual void RenderMainToolBar();

    virtual void OnKeyPress(int glfw_key, int scancode, int action, int mods);
    virtual void OnWindowMoved(int x, int y);

    virtual bool OnWindowCreated();
    virtual void OnWindowDestroyed();

    void SetEnableStatusBar(bool enabled) { enable_statusbar = enabled; }
    void SetEnableToolBar(bool enabled) { enable_toolbar = enabled; }
    void SetWindowPos(int x, int y);

    // Autodocking new windows requires these utility functions
    // Do not save the ID return values, as they may change when the dockspace builder is recreated
    bool HasDockBuilder() const { return has_dock_builder; }
    unsigned int GetDockspaceImGuiID() const { return imgui_dockspace_id; }
    unsigned int GetDockBuilderRootID() const { return imgui_dock_builder_root_id; }
    unsigned int GetDockBuilderLeftID() const { return imgui_dock_builder_left_id; }
    unsigned int GetDockBuilderRightID() const { return imgui_dock_builder_right_id; }
    unsigned int GetDockBuilderBottomID() const { return imgui_dock_builder_bottom_id; }

    unsigned int imgui_dockspace_id;
    unsigned int imgui_dock_builder_root_id;
    unsigned int imgui_dock_builder_left_id;
    unsigned int imgui_dock_builder_right_id;
    unsigned int imgui_dock_builder_bottom_id;

    bool has_dock_builder;

protected:
    Application(std::string const& _window_title, int _window_width, int _window_height);

protected:
    ImVec4 clear_color;

    void ShowDockSpace(bool dockSpaceHasBackground = true);

private:
    int CreateWindow();
    void DestroyWindow();
    void KeyPressHandler(GLFWwindow*, int, int, int, int);
    void WindowPosHandler(GLFWwindow*, int, int);
    void _RenderMainStatusBar();
    void _RenderMainToolBar();

    GLFWwindow* glfw_window;
    int         window_width;
    int         window_height;
    std::string window_title;
    bool        enable_statusbar;
    bool        enable_toolbar;

};
