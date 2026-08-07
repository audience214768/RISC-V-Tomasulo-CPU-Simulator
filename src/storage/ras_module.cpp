#include "storage/ras_module.hpp"

void RASModule::eval(const FetchRASWritePorts &fetch, const FlushRASWritePorts &flush) {
    for (int i = 0; i < RAS_SIZE; i++) {
        stack_[i].hold();
    }
    head_.hold();

    u32 h = head_.cur();

    // P1: Flush restore — the flushed window's undo only moves the head, 
    // so restoring the head is sufficient.
    if (flush.restore_valid.read()) {
        h = flush.restore_head.read();
        head_.next_raw() = h;
    }

    // P2: Fetch-stage op, applied on top of the (possibly restored) state.
    if (fetch.pop_valid.read() && h > 0) {
        h--;
        head_.next_raw() = h;
    }
    if (fetch.push_valid.read() && h < RAS_SIZE) {
        stack_[h].next_raw() = fetch.push_val.read();
        head_.next_raw() = h + 1;
    }
}
