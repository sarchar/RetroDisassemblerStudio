#pragma once

#include "wires.h"

#include <iostream>
template <typename T>
class SignalDelay
{
public:
    SignalDelay(bool _edge, unsigned int _delay, unsigned int _total_clocks) 
        : edge(_edge), delay(_delay), total_clocks(_total_clocks), counter(0)
    {
        *pins.clk.signal_changed += [=, this](Wire*, std::optional<bool> const& new_state) {
            if(*new_state != this->edge) return;

            unsigned int c = counter++;

            if(c == this->delay) {
                this->Transfer();
            } 

            if(c == (this->total_clocks - 1)) {
                counter = 0;
            }
        };

        *pins.reset_n.signal_changed += [=, this](Wire*, std::optional<bool> const& new_state) {
            if(!*new_state) {
                this->counter = 0;
                this->Transfer(); // TODO not sure if this is necessary
            }
        };
    }

    inline void Transfer()
    {
        // pass signal from in to out
        pins.out.Assert(pins.in.Sample());
    }

    struct {
        Wire clk     { "SignalDelay.clk" };
        Wire reset_n { "SignalDelay.reset_n" };
        Bus<T> in    { "SignalDelay.in" };
        Bus<T> out   { "SignalDelay.out" };
    } pins;

private:
    unsigned int counter;
    bool         edge;
    unsigned int delay;
    unsigned int total_clocks;

};
