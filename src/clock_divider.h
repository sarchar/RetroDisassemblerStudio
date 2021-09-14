#pragma once

#include "system_clock.h"
#include "wires.h"

class ClockDivider {
public:
    ClockDivider(unsigned int _divider, bool _edge, bool start_high)
        : half_divider(_divider >> 1), edge(_edge), counter(0)
    {
        assert((_divider % 2) == 0);
    
        // set up the starting clock position
        if(start_high) pins.out.AssertHigh();
        else           pins.out.AssertLow();
    
        // every clock cycle edge that we care about, increment the counter and flip the clock
        *pins.in.signal_changed += [=, this](Wire*, std::optional<bool> const& new_state) {
            // only watch the required edge
            if(*new_state == this->edge) {
                unsigned int c = ++this->counter;
                if(c == 1) {
                    pins.out.Assert(!pins.out.Sample());
                } 
                if(c == half_divider) {
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
    bool edge;
    unsigned int half_divider;
    unsigned int counter;
};

