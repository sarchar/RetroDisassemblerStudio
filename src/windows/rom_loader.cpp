#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <string>
#include <thread>

#include "imgui.h"
#include "imgui_internal.h"

#include "main.h"
#include "project.h"
#include "signals.h"
#include "systems/system.h"
#include "windows/rom_loader.h"

using namespace std;

shared_ptr<ProjectCreatorWindow> ProjectCreatorWindow::CreateWindow(string const& _file_path_name)
{
    return make_shared<ProjectCreatorWindow>(_file_path_name);
}

ProjectCreatorWindow::ProjectCreatorWindow(string const& _file_path_name)
    : BaseWindow("project_creater"),
      file_path_name(_file_path_name),
      loader_state(LOADER_STATE_INIT)
{
    SetTitle("Project Creator");
    SetWindowless(true);
    project_created = make_shared<project_created_t>();
}

ProjectCreatorWindow::~ProjectCreatorWindow()
{
    if(create_project_thread) create_project_thread->join();
}

void ProjectCreatorWindow::UpdateContent(double deltaTime) 
{
    // loop over all projects asking if the file name is valid
    // if there's only 1 valid project, load it
    // if there's more than 1, ask the user to clarify which system should load it
    // returns a Project instance
    switch(loader_state) {
    case LOADER_STATE_INIT:
    {
        // open the rom file
        ifstream is(file_path_name, ios::binary);
        if(!is) {
            loader_state = LOADER_STATE_FILE_NOT_FOUND;
            break;
        }

        // loop over all the loaders and accumuilate the valid ones
        vector<BaseProject::Information const*> valid_projects;
        int i = 0;
        while(auto info = BaseProject::GetProjectInformation(i++)) {
            if(info->is_rom_valid(file_path_name, is)) {
                valid_projects.push_back(info);
            }

            // rewind the pointer for the next call
            is.clear();
            is.seekg(0);
        }

        // if there are no valid loaders, tell the user
        if(valid_projects.size() == 0) {
            loader_state = LOADER_STATE_NOT_A_VALID_ROM;
            break;
        }

        // if there's only 1 valid loader, load it
        if(valid_projects.size() == 1) {
            CreateNewProject(valid_projects[0]);
            loader_state = LOADER_STATE_CREATING_PROJECT;
            //TODO CloseWindow();
            break;
        }
        
        // otherwise, allow the user to select which system to load
        available_systems = valid_projects;
        loader_state = LOADER_STATE_SELECT_SYSTEM;
        break;
    }
    default:
        break;
    }
}

void ProjectCreatorWindow::RenderContent() 
{
    switch(loader_state) {
    case LOADER_STATE_NOT_A_VALID_ROM:
        if(MyApp::Instance()->OKPopup("Project Creator##notvalid", "The selected ROM file is not valid with any supported retro system.")) {
            CloseWindow();
        }
        break;

    case LOADER_STATE_FILE_NOT_FOUND:
        if(MyApp::Instance()->OKPopup("Project Creator##notfound", "The selected ROM file was not found or could not be opened for reading.")) {
            CloseWindow();
        }
        break;

    case LOADER_STATE_SELECT_SYSTEM:
    {
        static int current_selection = 0;

        // Always open it
        string title = "Project Creator - Select System";
        ImGui::OpenPopup(title.c_str());

        // center this window
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize;
        if(ImGui::BeginPopupModal(title.c_str(), nullptr, flags)) {
            //TextWrapped doesn't work well
            //ImGui::PushTextWrapPos(ImGui::GetContentRegionAvailWidth());
            ImGui::Text("Multiple systems appear valid. Select which system to use to load the ROM.");
            for(int i = 0; i < available_systems.size(); ++i) {
                auto info = available_systems[i];
                stringstream ss;
                ss << (i + 1) << ". " << info->full_name;
                if (ImGui::RadioButton(ss.str().c_str(), current_selection == i)) { 
                    current_selection = i; 
                }
            }
            
            if(ImGui::Button("OK")) {
                auto info = available_systems[current_selection];
                CreateNewProject(info);
                CloseWindow();
            }
            //ImGui::PopTextWrapPos();
            ImGui::EndPopup();
        }
        break;
    }

    case LOADER_STATE_CREATING_PROJECT:
    {
        string title = "Project Creator";
        ImGui::OpenPopup(title.c_str());

        // center this window
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize;
        if(ImGui::BeginPopupModal(title.c_str(), nullptr, flags)) {
            if(create_project_max_progress != 0) {
                ImGui::Text("%s (%.2f%%)", create_project_message.c_str(), create_project_current_progress / (float)create_project_max_progress * 100.0f);
            } else {
                // might be empty for a frame or two, but that's OK
                ImGui::Text("%s", create_project_message.c_str());
            }

            if(create_project_done) {
                if(!create_project_error || ImGui::Button("Close")) { // wait for OK to be pressed
                    project_created->emit(shared_from_this(), current_project);
                    create_project_done = false;
                }
            }
        }

        ImGui::EndPopup();
        break;
    }

    default:
        break;
    }
}

void ProjectCreatorWindow::CreateNewProject(BaseProject::Information const* info)
{
    current_project = info->create_project();
    *current_project->create_new_project_progress += std::bind(&ProjectCreatorWindow::CreateNewProjectProgress, this, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4, placeholders::_5);
    create_project_max_progress = 0;
    create_project_message.clear();
    create_project_error = false;
    create_project_done = false;

    // Create a loading thread for the project file creation
    create_project_thread = make_unique<std::thread>(std::bind(&ProjectCreatorWindow::CreateProjectThreadMain, this));
}

void ProjectCreatorWindow::CreateNewProjectProgress(shared_ptr<BaseProject> system, bool error, u64 max_progress, u64 current_progress, std::string const& msg)
{
    cout << "[ProjectCreatorWindow] CreateNewProjectProgress: " << msg << " (" << current_progress << "/" << max_progress << ")" << endl;

    create_project_error            = error;
    create_project_max_progress     = max_progress;
    create_project_current_progress = current_progress;
    create_project_message          = msg;
}

void ProjectCreatorWindow::CreateProjectThreadMain()
{
    cout << "[ProjectCreatorWindow] CreateProjectThreadMain start" << endl;

    if(current_project->CreateNewProjectFromFile(file_path_name)) {
        // success
    } else {
        // failure
    }

    cout << "[ProjectCreatorWindow] CreateProjectThreadMain done" << endl;

    create_project_done = true;
}

