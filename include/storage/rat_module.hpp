#pragma once
#include "rtl/register.hpp"
#include "ports/rat_ports.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"

class RATModule {
public:
    RATModule() { reset(); }

    void drive_read_ports(RATReadPorts &p) {
        for (int i = 0; i < NUM_ARCH_REGS; i++) {
            p.map[i].write(map_[i].cur());
        }
    }

    void eval(
        const IssueRATWritePorts &issue,
        const FlushRATWritePorts &flush
    );

    void tick() { 
        for (int i = 0; i < NUM_ARCH_REGS; i++) {
            map_[i].tick(); 
        }
    }
    void reset() { 
        for (int i = 0; i < NUM_ARCH_REGS; i++) {
            map_[i].reset(i); 
        }
    }

private:
    Register<32> map_[NUM_ARCH_REGS];
};
