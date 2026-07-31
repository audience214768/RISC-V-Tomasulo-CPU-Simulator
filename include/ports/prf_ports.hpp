#pragma once
#include "rtl/wire.hpp"
#include "utils/config.hpp"

struct PRFReadPorts {
    Wire<32> data[NUM_PHYS_REGS];
};

struct WBPRFWritePorts {
    static constexpr int kMaxWrites = CDB_SIZE;
    Wire<1>  valid[kMaxWrites];
    Wire<32> preg[kMaxWrites];
    Wire<32> data[kMaxWrites];
    void clear() {
        wire_clear(valid); wire_clear(preg); wire_clear(data);
    }
};
