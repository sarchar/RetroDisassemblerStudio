#pragma once

#include <cassert>
#include <string>

#include "signals.h"

typedef s8 tristate; // 0 = low, 1 = high, -1 = highz

class Wire {
public:
    Wire(std::string const& _name) 
        : driver(nullptr), state(-1), name(_name) 
    { 
        signal_changed = std::make_shared<signal_changed_t>();
    }

    void Connect(Wire* other) {
        // when a new wire is attached, we either take our signal and tell them or take their signal
        // but if both are driven then we have a problem
        assert(state == -1 || other->state == -1);

        // if they're both -1, it's fine to just take theirs
        if(state == -1) { // if our state is undefined, take theirs even if it's high-z
            IncomingSignal(other->driver, other->state);
        } else {
            // otherwise our state is defined, tell them before attaching the signal
            other->IncomingSignal(driver, state);
        }

        // queue our signal to tell other when our state changes
        *signal_changed += [other](Wire* driver, tristate new_state) {
            other->IncomingSignal(driver, new_state);
        };

        // queue their signal to tell us when their state changes
        *other->signal_changed += [=, this](Wire* driver, tristate new_state) {
            this->IncomingSignal(driver, new_state);
        };
    }

    inline void Assert(tristate new_state) {
        // changing the signal on our line when its being driven by something else is a problem
        assert(!(state != -1 && new_state >= 0 && (driver != this)));

        // if state doesn't change we don't do anything
        if(new_state == state) return;

        // update state
        driver = (new_state >= 0) ? this : nullptr;
        state = new_state;

        // tell all connections
        signal_bounce = true;
        signal_changed->emit(driver, new_state);
        signal_bounce = false;
    }

    inline void AssertLow()  { Assert(0); }
    inline void AssertHigh() { Assert(1); }
    inline void HighZ()      { Assert(-1); }
    inline bool Sample()     { return ((state < 0) ? ((bool)((uintptr_t)this & 0x01)) : (bool)state); } // in highz you get random results

    // all connections to other wires are simply in the signal that are connected
    // signal_changed(Wire* driver, tristate state)
    typedef signal<std::function<void(Wire*, tristate)>> signal_changed_t;
    std::shared_ptr<signal_changed_t> signal_changed;

protected:
    inline void IncomingSignal(Wire* new_driver, tristate new_state) {
        if(signal_bounce) return;

        // if state doesn't change we don't propagate the new signal
        if(new_state == state) {
            assert(new_driver == driver); // wanna catch the case where something else asserts the same signal
            return;
        }

        // any incoming signal when we're being driven better be the same driver
        // otherwise we have a wire conflict
        assert(driver == nullptr || driver == new_driver);

        // update state
        driver = (new_state >= 0) ? new_driver : nullptr; 
        state = new_state;

        // prevent signal from coming back to us
        signal_bounce = true;
        signal_changed->emit(new_driver, new_state); // tell all our peers
        signal_bounce = false;
    }

private:
    // the current state on this wire
    tristate state;

    // set to ourselves if Assert() was called or set to the driver in IncomingSignal or null if high-z
    Wire* driver;

    // a name for the wire or pin
    std::string name;

    // signal bounce 555
    bool signal_bounce = false;
};

//template <typename N>
//class Bus {
//private:
//    Wire pins[N];
//};

