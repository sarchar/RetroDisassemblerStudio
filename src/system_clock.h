#pragma once

#include "util.h"
#include "wires.h"

class SystemClock {
public:
    SystemClock(u64 _frequency);
    ~SystemClock();

    void Step();
    void HalfStep();
    void StepToHigh();
    void StepToLow();
    void Enable();
    void Disable();

    struct {
        Wire out      { "SystemClock.out" };
        Wire enable_n { "SystemClock.enable_n" };
    } pins;

private:
    u64 frequency;
    bool last_state;
    bool enabled;
};
