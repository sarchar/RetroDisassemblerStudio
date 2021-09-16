// Registered ROM
// The SNES has asynchronous rom, but cpus are inherently synchronous
// and while possible to emulate, it might be too much for this project. We're 
// simulating the rom by clocking it in the middle of the CPU clock cycle after 
// address lines are setup
//
// Started as a copy of RAM, but since the modules are basic enough I don't
// see the point in using inheritance and figuring out a good way to inherit pins
// and changing the names, etc.
#pragma once

#include "util.h"
#include "wires.h"

template <typename A, typename D>
class ROM 
{
public:
    // _po2_size = power of 2 size
    // rom will be 2^_po2_size * sizeof(D) bytes
    ROM(u8 _po2_size, bool _edge);
    ~ROM();

    void LoadImage(u8 const* data, A load_address, A size);

    struct {
        Wire   clk  { "ROM.clk"  };
        Wire   cs_n { "ROM.cs_n" };
        Bus<A> a    { "ROM.a"    };
        Bus<D> d    { "ROM.d"    };
    } pins;

private:
    D*   memory;
    A    mask;
    bool selected;
    bool edge;

    void Latch();
};

template <typename A, typename D>
ROM<A, D>::ROM(u8 _po2_size, bool _edge)
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
        // the data bus to high-z. this is the only part of ROM that is not clocked
        if(selected && *new_state) {
            pins.d.HighZ();
        }
        selected = !*new_state;
    };
}

template <typename A, typename D>
ROM<A, D>::~ROM()
{
    delete [] memory;
}

template <typename A, typename D>
void ROM<A, D>::Latch()
{
    if(!selected) return;

    A addr = pins.a.Sample() & mask;
    std::cout << "[ROM] latch: reading ROM address $" << std::hex << addr << " value = $" << memory[addr] << std::endl;
    pins.d.Assert(memory[addr]);
}

template <typename A, typename D>
void ROM<A, D>::LoadImage(u8 const* data, A load_address, A size)
{
    load_address &= mask;
    load_address += size;

    while(size != 0) {
        memory[--load_address] = data[--size];
    }
}
