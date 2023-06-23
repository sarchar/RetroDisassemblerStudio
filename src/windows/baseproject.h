// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "signals.h"
#include "util.h"

#include "windows/basewindow.h"

#define PROJECT_FILE_MAGIC         0x8781A90AFDE1F317ULL
#define PROJECT_FILE_VERSION       FILE_VERSION_LAST
#define PROJECT_FILE_DEFAULT_FLAGS 0

// Add a new flag equal to the version number above
// And to check if your save file has support for the feature:
// if(GetCurrentProject()->GetSaveFileVersion >= FILE_VERSION_SAVE_STATES) ...
// The checks are used in Load*() only. The Save functions should always save
// the latest format
enum FILE_VERSIONS {
    FILE_VERSION_BASE        = 0x00000101,
    FILE_VERSION_SAVE_STATES = 0x00000102,   // support for Save States
    FILE_VERSION_ENUMS       = 0x00000103,   // support for Enums
    FILE_VERSION_READVARINT2 = 0x00000104,   // support for negative and 64-bit values in Read/WriteVarInt
    FILE_VERSION_FLATMEMORY  = 0x00000105,   // change memory objects view of memory to MemoryRegion::flat_memory
    FILE_VERSION_COMMENTS    = 0x00000106,   // comments with expressions
    FILE_VERSION_BLANKLINES  = 0x00000107,   // custom blank lines
    FILE_VERSION_QUICKEXP    = 0x00000108,   // quick expressions
    FILE_VERSION_ENUMSIZE    = 0x00000109,   // changeable enum sizes

    // update me every time a new file version is added
    FILE_VERSION_LAST = FILE_VERSION_ENUMSIZE
};

class BaseSystem;

namespace Windows {

class BaseProject : public BaseWindow {
public:
    struct Information {
        std::string abbreviation;
        std::string full_name;
        std::function<bool(std::string const&, std::istream&)> is_rom_valid;
        std::function<std::shared_ptr<BaseProject>(int, int)> create_project;
    };
    virtual Information const* GetInformation() = 0;

    BaseProject(std::string const&, int, int);
    virtual ~BaseProject();

    std::string                 GetRomFileName() const { return rom_file_name; }
    std::shared_ptr<BaseSystem> GetBaseSystem() { return current_system; }

    int GetSaveFileVersion() const { return save_file_version; }
    int GetSaveFileFlags()   const { return save_file_flags; }

    template <class T>
    std::shared_ptr<T> GetSystem() {
        return dynamic_pointer_cast<T>(GetBaseSystem());
    }

    // slow, call from separate thread
    virtual bool CreateNewProjectFromFile(std::string const&) = 0;

    virtual void CreateSystemInstance() = 0;

    virtual bool Save(std::ostream&, std::string&);
    virtual bool Load(std::istream&, std::string&);
    static std::shared_ptr<BaseProject> StartLoadProject(std::istream&, std::string&, int, int);

    // signals
    make_signal(create_new_project_progress, void(std::shared_ptr<BaseProject>, bool error, 
            u64 max_progress, u64 current_progress, std::string const& msg));

protected:
    virtual void ChildWindowAdded(std::shared_ptr<BaseWindow> const&) {}
    virtual void ChildWindowRemoved(std::shared_ptr<BaseWindow> const&) {}

    std::shared_ptr<BaseSystem> current_system;
    std::string                 rom_file_name;

    int save_file_version;
    int save_file_flags;

public:
    static void RegisterProjectInformation(Information const*);
    static Information const* GetProjectInformation(int);
    static Information const* GetProjectInformation(std::string const& abbreviation);
    static std::vector<Information const*> project_informations;

};

}
