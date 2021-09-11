#pragma once

#include "base_window.h"
#include "systems/system.h"

#include <memory>
#include <string>

class ROMLoader : public BaseWindow {
public:
    ROMLoader(std::string const& _file_path_name);
    virtual ~ROMLoader();

    static ROMLoader* CreateWindow(std::string const& _file_path_name);
    //std::shared_ptr<System> CreateSystem();

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;

private:
    std::string file_path_name;
};
