#pragma once
#include "rtl/register.hpp"
#include "ports/ras_ports.hpp"
#include "utils/config.hpp"

class RASModule {
    Register<32> stack_[RAS_SIZE];
    Register<6> head_;

public:
    RASModule() { reset(); }

    void drive_read_ports(RASReadPorts &p) const {
        p.empty.write(head_.cur() == 0 ? 1 : 0);
        p.top.write(head_.cur() > 0 ? stack_[head_.cur() - 1].cur() : 0);
        p.head.write(head_.cur());
        for (int i = 0; i < RAS_SIZE; i++) {
            p.stack[i].write(stack_[i].cur());
        }
    }

    void eval(const FetchRASWritePorts &fetch, const FlushRASWritePorts &flush);

    void tick() {
        for (int i = 0; i < RAS_SIZE; i++) {
            stack_[i].tick();
        }
        head_.tick();
    }

    void reset() {
        for (int i = 0; i < RAS_SIZE; i++) {
            stack_[i].reset(0);
        }
        head_.reset(0);
    }
};
