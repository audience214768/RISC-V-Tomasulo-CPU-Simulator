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

// Flush restore: undoing the flushed window's fetch-stage RAS ops only
// moves the head (call pushes are popped back, return pops are re-pushed;
// the stack contents are never rewritten), so restoring the head pointer
// undoes the whole window in one combinational step. The walker (RAT /
// free list / ready table rollback) does not touch the RAS.
struct FlushRASWritePorts {
    Wire<1> restore_valid;
    Wire<6> restore_head;
    void clear() { restore_valid.write(0); }
};
