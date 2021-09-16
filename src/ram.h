// Registered RAM
// The SNES has asynchronous static ram, but cpus are inherently synchronous
// and while possible to emulate, it might be too much for this project. We're 
// simulating the ram by clocking it in the middle of the CPU clock cycle after 
// address lines are setup
#pragma once

#include "util.h"
#include "wires.h"

template <typename A, typename D>
class RAM 
{
public:
    // _po2_size = power of 2 size
    // ram will be 2^_po2_size * sizeof(D) bytes
    RAM(u8 _po2_size, bool _edge);
    ~RAM();

    struct {
        Wire   clk  { "RAM.clk"  };
        Wire   cs_n { "RAM.cs_n" };
        Wire   rw_n { "RAM.rw_n" };
        Bus<A> a    { "RAM.a"    };
        Bus<D> d    { "RAM.d"    };
    } pins;

private:
    D*   memory;
    A    mask;
    bool selected;
    bool edge;

    void Latch();
};

template <typename A, typename D>
RAM<A, D>::RAM(u8 _po2_size, bool _edge)
    : selected(false), edge(_edge)
{
    memory = new D[1 << _po2_size];
    mask   = (1 << _po2_size) - 1;

    // default output pins
    pins.d.HighZ();

    *pins.clk.signal_changed += [=, this](Wire*, std::optional<bool> const& new_state) {
        if(*new_state == this->edge) {
            this->Latch();
        }
    };

    *pins.cs_n.signal_changed += [=, this](Wire*, std::optional<bool> const& new_state) {
        // if we were previously selected and the new state deselects, immediately return
        // the data bus to high-z. this is the only part of RAM that is not clocked
        if(selected && *new_state) {
            pins.d.HighZ();
        }
        selected = !*new_state;
    };
}

template <typename A, typename D>
RAM<A, D>::~RAM()
{
    delete [] memory;
}

template <typename A, typename D>
void RAM<A, D>::Latch()
{
    if(!selected) return;

    std::cout << "[RAM] latch: ";

    A addr = pins.a.Sample() & mask;

    if(pins.rw_n.Sample()) { // read
        std::cout << "reading RAM address $" << std::hex << addr << std::endl;
        pins.d.Assert(memory[addr]);
    } else { // write
        std::cout << "writing RAM address $" << std::hex << addr << std::endl;
        memory[addr] = pins.d.Sample();
    }
}

