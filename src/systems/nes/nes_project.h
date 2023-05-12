#pragma once

#include <string>

#include "project.h"

namespace NES {

class Project : public BaseProject {
public:
    BaseProject::Information const* GetInformation();

    Project();
    virtual ~Project();

    bool CreateNewProjectFromFile(std::string const&) override;

    void CreateDefaultWorkspace() override;

    // creation interface
    static BaseProject::Information const* GetInformationStatic();
    static bool IsROMValid(std::string const&, std::istream&);
    static std::shared_ptr<BaseProject> CreateProject();

    // Save and Load
    bool Save(std::ostream& os, std::string&) override;
    bool Load(std::istream& is, std::string&) override;

private:
};

}
