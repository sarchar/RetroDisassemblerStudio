#pragma once

#include <memory>

#include "signals.h"
#include "windows/base_window.h"

namespace NES {

class Listing : public BaseWindow {
public:
    Listing();
    virtual ~Listing();

    virtual char const * const GetWindowClass() { return Listing::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::Listing"; }

    // signals

protected:
    void UpdateContent(double deltaTime) override;
    void PreRenderContent() override;
    void RenderContent() override;

    void CheckInput();

private:

public:
    static std::shared_ptr<Listing> CreateWindow();
};

}
