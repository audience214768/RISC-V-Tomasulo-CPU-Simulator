#pragma once
#include "rtl/register.hpp"
#include "ports/ready_table_ports.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"

class ReadyTableModule {
public:
    ReadyTableModule() { reset(); }

    void drive_read_ports(ReadyTableReadPorts &p) {
        for (int i = 0; i < NUM_PHYS_REGS; i++) {
            p.ready[i].write(bits_[i].cur());
        }
    }

    void eval(
        const WBReadyWritePorts &wb,
        const IssueReadyWritePorts &issue,
        const FlushReadyWritePorts &flush
    );

    void force(PhysRegNum preg, bool val) { bits_[preg].next_raw() = val ? 1 : 0; }
    void tick() { 
        for (int i = 0; i < NUM_PHYS_REGS; i++) {
            bits_[i].tick(); 
        }
        //fprintf(stderr, "41 is %d\n", bits_[41].cur());
    }
    void reset() {
        for (int i = 0; i < NUM_ARCH_REGS; i++) {
            bits_[i].reset(1);
        }
        for (int i = NUM_ARCH_REGS; i < NUM_PHYS_REGS; i++) {
            bits_[i].reset(0);
        }
    }

private:
    Register<1> bits_[NUM_PHYS_REGS];
};
