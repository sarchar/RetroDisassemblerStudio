#pragma once

#include <cassert>
#include <iostream>
#include <optional>
#include <string>

#include "signals.h"
#include "util.h"

// Bus now something that transmits a T or nothing
template <typename T>
class Bus {
public:
    Bus(std::string const& _name) 
        : driver(nullptr), name(_name), signal_bounce(0) 
    { 
        signal_changed = std::make_shared<signal_changed_t>();
    }

    void Connect(Bus<T>* other) {
        // when a new wire is attached, we either take our signal and tell them or take their signal
        // but if both are driven then we have a problem
        assert(!state.has_value() || !other->state.has_value());

        // if they're both -1, it's fine to just take theirs
        if(!state.has_value()) { // if our state is undefined, take theirs even if it's high-z
            IncomingSignal(other->driver, other->state);
        } else {
            // otherwise our state is defined, tell them before attaching the signal
            other->IncomingSignal(driver, state);
        }

        // queue our signal to tell other when our state changes
        *signal_changed += [other](Bus<T>* driver, std::optional<T> const& new_state) {
            other->IncomingSignal(driver, new_state);
        };

        // queue their signal to tell us when their state changes
        *other->signal_changed += [=, this](Bus<T>* driver, std::optional<T> const& new_state) {
            this->IncomingSignal(driver, new_state);
        };
    }

    inline void Assert(std::optional<T> const& new_state) {
        // changing the signal on our line when its being driven by something else is a problem
        assert(!(state.has_value() && new_state.has_value() && (driver != this)));
        //if(new_state.has_value()) std::cout << name << " asserted $" << std::hex << (unsigned int)*new_state << std::endl;
        //else                      std::cout << name << " set high-z" << std::endl;

        // if state doesn't change we don't do anything
        if(new_state == state) return;

        // if someone else is driving the line when we go high-z, don't care
        if(!new_state.has_value() && driver != this) return;

        // update state
        driver = new_state.has_value() ? this : nullptr;
        state = new_state;

        // tell all connections
        signal_bounce = true;
        signal_changed->emit(driver, new_state);
        signal_bounce = false;
    }

    // helper to get a high/low values and a mask for randomness that will never be used...
    template <typename S> struct type_helpers { 
        static unsigned long long const MASK = (1ULL << 8*sizeof(S)) - 1; 
        static S                  const LOW  = 0;
        static S                  const HIGH = ~(S)0;
    };

    template <>           struct type_helpers<bool> {  // specialize bool to use true and false and 1 bit
        static unsigned long long  const MASK = 0x01; 
        static bool                const LOW  = false;
        static bool                const HIGH = true;
    };

    inline void                    AssertLow()     { Assert(std::optional<T>(type_helpers<T>::LOW)); }
    inline void                    AssertHigh()    { Assert(std::optional<T>(type_helpers<T>::HIGH)); }
    inline void                    HighZ()         { Assert(std::optional<T>()); }
    inline T                       Sample()  const { return state.value_or((uintptr_t)this & type_helpers<T>::MASK); } // in highz you get random results
    inline bool                    IsHighZ() const { return !state.has_value(); }
    inline std::optional<T> const& Get()     const { return state; }

    // all connections to other wires are simply in the signal that are connected
    // signal_changed(Wire* driver, std::optional<T> const& new_state)
    typedef signal<std::function<void(Bus<T>*, std::optional<T> const&)>> signal_changed_t;
    std::shared_ptr<signal_changed_t> signal_changed;

protected:
    inline void IncomingSignal(Bus<T>* new_driver, std::optional<T> const& new_state) {
        if(signal_bounce) return;

        // if state doesn't change we don't propagate the new signal
        if(new_state == state) {
            assert(new_driver == driver); // wanna catch the case where something else asserts the same signal
            return;
        }

        // any incoming signal when we're being driven better be the same driver
        // otherwise we have a wire conflict
        assert(driver == nullptr || (!new_state.has_value() || driver == new_driver));

        // update state
        driver = new_state.has_value() ? new_driver : nullptr; 
        state = new_state;

        // prevent signal from coming back to us
        signal_bounce = true;
        signal_changed->emit(new_driver, new_state); // tell all our peers
        signal_bounce = false;
    }

private:
    // the current state on this wire
    std::optional<T> state {};

    // set to ourselves if Assert() was called or set to the driver in IncomingSignal or null if high-z
    Bus<T>* driver;

    // a name for the wire or pin
    std::string name;

    // signal bounce 555
    bool signal_bounce = false;
};

typedef Bus<bool> Wire;

//template <typename N>
//class Bus {
//private:
//    Wire pins[N];
//};

