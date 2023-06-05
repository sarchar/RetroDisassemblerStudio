// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <iostream>
#include <string>
#include <functional>

#include "signals.h"
#include "util.h"

class BaseSystem : public std::enable_shared_from_this<BaseSystem> {
public:
    BaseSystem();
    virtual ~BaseSystem();

    virtual bool Save(std::ostream& os, std::string&) = 0;
    virtual bool Load(std::istream&, std::string&) = 0;
};
