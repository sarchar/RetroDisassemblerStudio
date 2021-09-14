#include "systems/snes/snes_address_decoder.h"

SNESAddressDecoder::SNESAddressDecoder()
{
    pins.ram_cs_n.AssertHigh();

    *pins.a.signal_changed += [=, this](Bus<u16>*, std::optional<u16> const& new_state) {
        bool address_valid = this->pins.vda.Sample() || this->pins.vpa.Sample();
        if(address_valid) {
            this->pins.ram_cs_n.AssertLow();
        } else {
            this->pins.ram_cs_n.AssertHigh();
        }
    };

    //*pins.vda.signal_changed += [=, this](Bus<u16>*, std::optional<u16> const& new_state) {
    //    // if neither VDA nor VPA are asserted, return CSn high
    //    if(!*new_state) {
    //        if(!pins.vpa.Sample()) {
    //            this->pins.ram_cs_n.AssertHigh();
    //        }
    //    }
    //}
}
