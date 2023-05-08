#include <memory>

#include "system.h"

using namespace std;

System::System() 
{
    create_new_project_progress = make_shared<create_new_project_progress_t>();
}

System::~System()
{
}

