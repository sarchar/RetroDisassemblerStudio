#pragma once

#include "base_window.h"
#include "signals.h"
#include "systems/system.h"

#include <memory>
#include <string>

class ROMLoader : public BaseWindow {
public:
    ROMLoader(std::string const& _file_path_name);
    virtual ~ROMLoader();

    static std::shared_ptr<ROMLoader> CreateWindow(std::string const& _file_path_name);
    //std::shared_ptr<System> CreateSystem();

    // signals
    typedef signal<std::function<void(std::shared_ptr<BaseWindow>, std::shared_ptr<System>)>> system_loaded_t;
    std::shared_ptr<system_loaded_t> system_loaded;

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;

private:
    std::string file_path_name;
};
