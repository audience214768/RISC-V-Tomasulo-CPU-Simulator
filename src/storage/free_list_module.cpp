#include "storage/free_list_module.hpp"
#include <cstdio>
#include <cstdlib>

void FreeListModule::eval(
    const IssueFLWritePorts &issue,
    const CommitFLWritePorts &commit,
    const FlushFLWritePorts &flush
) {
    size_t nxt_head = head_.cur(), nxt_tail = tail_.cur(), nxt_count = count_.cur();
    for (int i = 0; i < NUM_PHYS_REGS; i++) {
        buf_[i].hold();
    }
    head_.hold();
    tail_.hold();
    count_.hold();

    // Priority: Flush pushes > Commit pushes > Issue pop
    for (u8 i = 0; i < static_cast<u8>(flush.push_count.read()); i++) {
        buf_[nxt_tail].next_raw() = flush.push_pregs[i].read();
        nxt_tail = (nxt_tail + 1) % NUM_PHYS_REGS;
        nxt_count++;
    }
    for (u8 i = 0; i < static_cast<u8>(commit.push_count.read()); i++) {
        buf_[nxt_tail].next_raw() = commit.push_pregs[i].read();
        nxt_tail = (nxt_tail + 1) % NUM_PHYS_REGS;
        nxt_count++;
    }
    if (issue.pop_req.read() && !issue.suppressed.read()) {
        if (nxt_count == 0) {
            fprintf(stderr, "FreeList: pop on empty!\n");
            exit(1);
        }
        nxt_head = (nxt_head + 1) % NUM_PHYS_REGS;
        nxt_count--;
    }

    head_.next_raw() = static_cast<u32>(nxt_head);
    tail_.next_raw() = static_cast<u32>(nxt_tail);
    count_.next_raw() = static_cast<u32>(nxt_count);
}
