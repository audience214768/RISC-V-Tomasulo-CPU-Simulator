#pragma once
#include "rtl/wire.hpp"
#include "utils/config.hpp"

// Memory write port: eval_commit drives it, the store is applied to
// mem_.buf at the clock edge (see TomasuloTop::tick), so a committed
// store becomes visible to loads from the next cycle on.
struct MemWritePorts {
    Wire<1> valid;
    Wire<32> addr;
    Wire<32> data;
    Wire<8> width;
    void clear() {
        valid.write(0);
    }
};
