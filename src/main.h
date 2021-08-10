#pragma once

#include "application.h"

class MyApp : public Application {
public:
    MyApp(int argc, char* argv[]);
    virtual ~MyApp();

protected:
    virtual bool Update(double deltaTime) override;
    virtual void RenderGUI() override;
    virtual bool OnWindowCreated() override;

private:
    void ShowDockSpace();
    void CreateMenuBar();

    bool request_exit;
};
