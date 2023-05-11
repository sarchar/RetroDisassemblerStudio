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


bool Save(ostream& os, string& errmsg)
{
    return false;
}

bool Read(istream& is, string& errmsg)
{
    return false;
}

