#pragma once
#include "rtl/register.hpp"
#include "ports/bht_ports.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"

class BHTModule {
public:
    BHTModule() { reset(); }

    void drive_read_ports(BHTReadPorts &p) const {
        for (int i = 0; i < BHT_SIZE; i++) {
            p.counters[i].write(counters_[i].cur());
        }
    }

    void eval(const ExecBHTWritePorts &exec);

    void tick() {
        for (int i = 0; i < BHT_SIZE; i++) {
            counters_[i].tick();
        }
    }

    void reset() {
        for (int i = 0; i < BHT_SIZE; i++) {
            counters_[i].reset(1);   // weak not-taken
        }
    }

private:
    Register<2> counters_[BHT_SIZE];
};
