#pragma once
#include "rtl/register.hpp"
#include "ports/free_list_ports.hpp"
#include "utils/config.hpp"
#include <cstdio>
#include <cstdlib>

class FreeListModule {
public:
    FreeListModule() { reset(); }

    void drive_read_ports(FreeListReadPorts &p) {
        p.empty.write(count_.cur() == 0 ? 1 : 0);
        p.head_val.write(count_.cur() > 0 ? buf_[head_.cur()].cur() : 0);
    }

    void eval(
        const IssueFLWritePorts &issue,
        const CommitFLWritePorts &commit,
        const FlushFLWritePorts &flush
    );

    void tick() {
        for (int i = 0; i < NUM_PHYS_REGS; i++) {
            buf_[i].tick();
        }
        head_.tick();
        tail_.tick();
        count_.tick();
    }
    void reset() {
        for (int i = 0; i < NUM_PHYS_REGS; i++) {
            buf_[i].reset(0);
        }
        head_.reset(0); 
        tail_.reset(0); 
        count_.reset(0);
        size_t t = 0;
        for (u32 i = NUM_ARCH_REGS; i < NUM_PHYS_REGS; i++) { 
            buf_[t].reset(i); 
            t++; 
        }
        tail_.reset(static_cast<u32>(t)); 
        count_.reset(static_cast<u32>(t));
    }

private:
    Register<32> buf_[NUM_PHYS_REGS], head_, tail_, count_;
};
