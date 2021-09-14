#pragma once

#include "wires.h"

class SNESAddressDecoder {
public:
    SNESAddressDecoder();
    
    struct {
        Wire vda      { "SNESAddressDecoder.vda" };
        Wire vpa      { "SNESAddressDecoder.vpa" };
        Bus<u16> a    { "SNESAddressDecoder.a" };
        Wire ram_cs_n { "SNESAddressDecoder.ram_cs_n" };
    } pins;
};
