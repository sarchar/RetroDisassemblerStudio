#include <memory>

#include "system.h"

using namespace std;

BaseSystem::BaseSystem() 
{
    create_new_project_progress = make_shared<create_new_project_progress_t>();
}

BaseSystem::~BaseSystem()
{
}

