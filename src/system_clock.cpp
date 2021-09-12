#include "system_clock.h"

SystemClock::SystemClock(u64 _frequency)
    : frequency(_frequency)
{
    // when enable line changes from high to low, make the clock pin go high-z -- this would 
    // allow something else to control the clock
    *pins.enable_n.signal_changed += [=, this](Wire* driver, tristate new_state) {
        if(this->enabled && new_state != 0) { // enable pin going high-z is same as disabled since we want a clear enable signal
            this->last_state = this->pins.out.Sample();
            this->pins.out.HighZ();
            this->enabled = false;
        } else if(!this->enabled && new_state == 0) {
            this->enabled = true;
            this->pins.out.Assert((tristate)this->last_state);
        }
    };

    // start with clock low
    pins.out.AssertLow();
    last_state = pins.out.Sample();

    //inverter.pins.in.Connect(pins.out);
    //inverter.pins.out.Connect(pins.out_b);
}

SystemClock::~SystemClock()
{
}

void SystemClock::Enable()
{
    pins.enable_n.AssertLow();
}

void SystemClock::Disable()
{
    pins.enable_n.AssertHigh();
}

void SystemClock::Step()
{
    HalfStep(); // Calling HalfStep() twice checks enable line on each half step
    HalfStep();
}

void SystemClock::HalfStep()
{
    if(pins.enable_n.Sample()) return;

    if(pins.out.Sample()) {
        pins.out.AssertLow();
    } else {
        pins.out.AssertHigh();
    }
}

void SystemClock::StepToHigh()
{
    if(pins.enable_n.Sample()) return;

    if(!pins.out.Sample()) pins.out.AssertHigh();
}

void SystemClock::StepToLow()
{
    if(pins.enable_n.Sample()) return;

    if(pins.out.Sample()) pins.out.AssertLow();
}

