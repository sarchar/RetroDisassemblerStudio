#pragma once

#include <iostream>
#include <string>
#include <functional>

#include "signals.h"
#include "util.h"

class BaseSystem : public std::enable_shared_from_this<BaseSystem> {
public:
   
public:
    BaseSystem();
    virtual ~BaseSystem();
};
