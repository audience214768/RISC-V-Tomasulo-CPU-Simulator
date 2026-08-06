#pragma once
#include "rtl/wire.hpp"
#include "utils/config.hpp"

// Memory write port: eval_commit drives it, MemoryModule::eval consumes it
// (hit -> merged into the line, dirty=1; miss -> store miss buffer).
struct MemWritePorts {
    Wire<1> valid;
    Wire<32> addr;
    Wire<32> data;
    Wire<8> width;
    void clear() {
        valid.write(0);
    }
};

// Memory read ports (driven by MemoryModule::drive_read_ports in step 0).
// Line / instruction data itself is read via MemoryModule::hit/byte/fetch_word
// (BHT-style combinational reads of the module state), not copied here.
struct MemReadPorts {
    Wire<1> refill_busy;   // a line refill is in flight: loads must not access
    Wire<1> sb_full;       // store miss buffer full: eval_commit must hold
};

// Load-miss refill request (driven by eval_memory, consumed by
// MemoryModule::eval)
struct MemRefillReqWritePorts {
    Wire<1> valid;
    Wire<32> addr;
    void clear() {
        valid.write(0);
    }
};
