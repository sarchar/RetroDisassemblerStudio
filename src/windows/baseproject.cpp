#include <iostream>
#include <memory>

#include "windows/baseproject.h"

using namespace std;

std::vector<BaseProject::Information const*> BaseProject::project_informations;

void BaseProject::RegisterProjectInformation(BaseProject::Information const* info)
{
    BaseProject::project_informations.push_back(info);
}

BaseProject::Information const* BaseProject::GetProjectInformation(int n)
{
    if(n < BaseProject::project_informations.size()) {
        return BaseProject::project_informations[n];
    }

    return NULL;
}

BaseProject::Information const* BaseProject::GetProjectInformation(std::string const& abbreviation)
{
    for(auto info : BaseProject::project_informations) {
        if(info->abbreviation == abbreviation) return info;
    }

    return NULL;
}

BaseProject::BaseProject(std::string const& title)
    : BaseWindow(title)
{
    SetWindowless(true);

    // local signals
    create_new_project_progress = make_shared<create_new_project_progress_t>();

    // connect signals
    auto app = MyApp::Instance();
    window_added_connection = 
        app->window_added->connect(std::bind(&BaseProject::_WindowAdded, this, placeholders::_1));

    window_removed_connection = 
        app->window_removed->connect(std::bind(&BaseProject::_WindowRemoved, this, placeholders::_1));
}

BaseProject::~BaseProject()
{
}


bool BaseProject::Save(ostream& os, string& errmsg)
{
    auto inf = GetInformation();

    // save the abbreviation designating which project type we are
    WriteString(os, inf->abbreviation);

    // save the ROM file location
    WriteString(os, rom_file_name);

    // TODO save workspace arrangement and docking locations

    if(!os.good()) {
        errmsg = "Failure writing BaseProject information";
        return false;
    }

    return true;
}

bool BaseProject::Load(std::istream& is, std::string& errmsg)
{
    ReadString(is, rom_file_name);
    cout << "BaseProject::rom_file_name = " << rom_file_name << endl;
    return is.good();
}

shared_ptr<BaseProject> BaseProject::LoadProject(std::istream& is, std::string& errmsg)
{
    string abbr;
    ReadString(is, abbr);

    auto info = BaseProject::GetProjectInformation(abbr);
    if(!info) {
        errmsg = "Could not find system: " + abbr;
        return nullptr;
    }

    cout << "Loading " << info->full_name << " project..." << endl;

    auto project = info->create_project();
    if(project->Load(is, errmsg)) return project;
    return nullptr;
}

