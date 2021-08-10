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

    virtual bool OnWindowCreated();
    virtual void OnWindowDestroyed();

protected:
    ImVec4 clear_color;

private:
    int CreateWindow();
    void DestroyWindow();

    GLFWwindow* glfw_window;
    int         window_width;
    int         window_height;
    std::string window_title;

};
