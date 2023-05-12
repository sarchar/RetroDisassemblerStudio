#include <iostream>
#include <memory>

#include "project.h"

using namespace std;

BaseProject::BaseProject()
{
    create_new_project_progress = make_shared<create_new_project_progress_t>();
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

bool BaseProject::Load(istream& is, string& errmsg)
{
    errmsg = "BaseProject::Load Unimplemented";
    return false;
}

