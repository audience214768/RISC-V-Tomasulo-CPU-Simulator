#pragma once
#include "rtl/register.hpp"
#include "ports/prf_ports.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"

class PRFModule {
    Register<32> values_[NUM_PHYS_REGS];
public:
    PRFModule() { reset(); }
    void drive_read_ports(PRFReadPorts &p) { 
        for(int i = 0; i < NUM_PHYS_REGS; i++) {
            p.data[i].write(values_[i].cur()); 
        }
    }
    void eval(const WBPRFWritePorts &wb);
    void force(PhysRegNum p, u32 v) { values_[p].next_raw() = v; }
    void tick() { 
        for(int i = 0; i < NUM_PHYS_REGS; i++) {
            values_[i].tick(); 
        }
    }
    void reset() { 
        for(int i = 0; i < NUM_PHYS_REGS; i++) {
            values_[i].reset(0); 
        }
    }
};
