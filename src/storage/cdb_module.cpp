#include "storage/cdb_module.hpp"
#include <cstdio>
#include <cstdlib>

void CDBModule::eval(
    const ExecCDBWritePorts &exec,
    const MemCDBWritePorts &mem,
    const WBCDBWritePorts &wb
) {
    for (int i = 0; i < CDB_SIZE; i++) {
        entries_[i].hold();
    }

    // Priority: Clear > Push (so cleared slots can be reused same cycle)
    for (int i = 0; i < CDB_SIZE; i++) {
        if (wb.clear_req[i].read()) {
            entries_[i].valid.next_raw() = 0;
        }
    }

    // Push: Execute (up to 4) + Memory (up to LSQ_SIZE)
    auto do_push = [&](u32 prd, u32 result, u32 rob_tag) {
        for (int i = 0; i < CDB_SIZE; i++) {
            if (entries_[i].valid.next_raw() == 0) {
                entries_[i].valid.next_raw() = 1;
                entries_[i].prd.next_raw() = prd;
                entries_[i].result.next_raw() = result;
                entries_[i].rob_tag.next_raw() = rob_tag;
                return;
            }
        }
        fprintf(stderr, "CDB: push on full!\n"); exit(1);
    };
    for (int j = 0; j < ExecCDBWritePorts::kMaxPush; j++) {
        if (exec.push_valid[j].read()) {
            do_push(exec.push_prd[j].read(), exec.push_result[j].read(), exec.push_rob_tag[j].read());
        }
    }
    for (int j = 0; j < MemCDBWritePorts::kMaxPush; j++) {
        if (mem.push_valid[j].read()) {
            do_push(mem.push_prd[j].read(), mem.push_result[j].read(), mem.push_rob_tag[j].read());
        }
    }
}
