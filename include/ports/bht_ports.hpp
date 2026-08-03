#pragma once
#include "rtl/wire.hpp"
#include "utils/config.hpp"

struct BHTReadPorts {
    Wire<2> counters[BHT_SIZE];
};

struct ExecBHTWritePorts {
    Wire<1>  update_req;
    Wire<32> update_idx;
    Wire<1>  update_taken;
    void clear() { update_req.write(0); }
};
