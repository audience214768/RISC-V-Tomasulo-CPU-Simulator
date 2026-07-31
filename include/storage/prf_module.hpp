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
    void eval(const WBPRFWritePorts &wb) {
        for(int i = 0; i < NUM_PHYS_REGS; i++) {
            values_[i].hold();
        }
        for(int j = 0; j < WBPRFWritePorts::kMaxWrites; j++) {
            if(wb.valid[j].read()) {
                //if (wb.preg[j].read() == 123) fprintf(stderr, "write to 123\n");
                values_[wb.preg[j].read()].next_raw() = wb.data[j].read();
            }
        }
    }
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
