#pragma once

#include "system_clock.h"
#include "wires.h"

class ClockDivider {
public:
    ClockDivider(unsigned int _divider, tristate _edge, bool start_high)
        : half_divider(_divider >> 1), edge(_edge), counter(0)
    {
        assert((_divider % 2) == 0);
    
        // set up the starting clock position
        if(start_high) pins.out.AssertHigh();
        else           pins.out.AssertLow();
    
        // every clock cycle edge that we care about, increment the counter and flip the clock
        *pins.in.signal_changed += [=, this](Wire*, tristate new_state) {
            // only watch the required edge
            if(new_state == _edge) {
                if(counter++ == half_divider) {
                    pins.out.Assert((tristate)!pins.out.Sample());
                    counter = 0;
                }
            }
        };
    }

    struct {
        Wire in  { "ClockDivider.in" };
        Wire out { "ClockDivider.out" };
    } pins;

private:
    tristate edge;
    unsigned int half_divider;
    unsigned int counter;
};

