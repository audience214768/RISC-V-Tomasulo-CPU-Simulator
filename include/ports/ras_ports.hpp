#pragma once
#include "rtl/wire.hpp"
#include "utils/config.hpp"

struct RASReadPorts {
    Wire<1> empty;
    Wire<32> top;              // stack_[head-1], combinational
    Wire<6> head;
    Wire<32> stack[RAS_SIZE];
};

// Fetch-stage ops: at most one push OR pop per cycle
// (call = push pc+4; return = pop, predicted target = popped value).
struct FetchRASWritePorts {
    Wire<1> push_valid;
    Wire<32> push_val;
    Wire<1> pop_valid;
    void clear() { push_valid.write(0); pop_valid.write(0); }
};

// Flush restore: undo of the flushed window only changes the head
// (calls are popped back, returns are re-pushed; stack content is
// untouched), so restoring the head pointer is sufficient.
struct FlushRASWritePorts {
    Wire<1> restore_valid;
    Wire<6> restore_head;
    void clear() { restore_valid.write(0); }
};
