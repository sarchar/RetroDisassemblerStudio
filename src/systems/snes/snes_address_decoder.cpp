#include "systems/snes/snes_address_decoder.h"

SNESAddressDecoder::SNESAddressDecoder()
{
    DeselectPeripherals();

    // Handle falling edge of reset
    *pins.reset_n.signal_changed += [=, this](Wire*, std::optional<bool> const& new_state) {
        if(!*new_state) {
            this->pins.db.HighZ();
            this->pins.d.HighZ();
            this->DeselectPeripherals();
        }
    };

    // Latch the bank on the rising edge of phi2 
    *pins.phi2.signal_changed += [=, this](Wire*, std::optional<bool> const& new_state) {
        // only interested in rising phi2 when not in reset
        if(!*new_state || !this->pins.reset_n.Sample()) return;

        // assert the output address lines
        u8 bank = pins.db.Sample();
        u32 address = (u32)bank << 16 | (u32)pins.a_in.Sample();
        pins.a_out.Assert(address);

        // on a read request, set the system data line to high z
        // for write requests, wait for the db line to change
        bool rw_n = pins.rw_n.Sample();
        if(rw_n) {
            pins.d.HighZ();
        }

        // determine if the address is valid
        bool address_valid = this->pins.vda.Sample() || this->pins.vpa.Sample();

        // return with no device activated on invalid addresses
        if(!address_valid) {
            DeselectPeripherals();
            return;
        }

        // valid address, set up the device to respond
        this->SelectPeripheral(address, rw_n);
    };

    // whenever the CPU goes to write, deassert db
    *pins.rw_n.signal_changed += [=, this](Wire*, std::optional<bool> const& new_state ) {
        pins.db.HighZ();
    };

    // on reads transmit the state of the system d line to the cpu db line
    *pins.d.signal_changed += [=, this](Bus<u8>*, std::optional<u8> const& new_state) {
        if(pins.rw_n.Sample()) {
            pins.db.Assert(new_state);
        }
    };

    // on writes transmit the state of the cpu db line to the system
    *pins.db.signal_changed += [=, this](Bus<u8>*, std::optional<u8> const& new_state) {
        if(!pins.rw_n.Sample()) {
            pins.d.Assert(pins.db.Get());
        }
    };

    // when the VDA and VPA pins change immediately respond to valid addresses going invalid
    auto deselect_peripherals = [=, this](Wire*, std::optional<bool> const& new_state) {
        if(!this->pins.vda.Sample() && !this->pins.vpa.Sample()) {
            this->DeselectPeripherals();
        }
    };

    *pins.vda.signal_changed += deselect_peripherals;
    *pins.vpa.signal_changed += deselect_peripherals;
}

void SNESAddressDecoder::SelectPeripheral(u32 address, bool rw_n)
{
    // TODO determine which device to select
    if((address & 0xFFFF0000) != 0) {
        DeselectPeripherals();
        return;
    }

    if(address & 0x8000) {
        // ROM only gets READ signals
        if(rw_n) {
            pins.rom_cs_n.AssertLow();
        }
    } else {
        pins.ram_cs_n.AssertLow();
    }
}

void SNESAddressDecoder::DeselectPeripherals()
{
    pins.ram_cs_n.AssertHigh();
    pins.rom_cs_n.AssertHigh();
}

