#include "storage/lsq_module.hpp"

void LSQModule::eval(
    const IssueLSQWritePorts &issue,
    const LSQPnumWritePorts &pnum,
    const ExecLSQWritePorts &exec,
    const MemLSQWritePorts &mem,
    const CommitLSQWritePorts &commit
) {
    for (int i = 0; i < LSQ_SIZE; i++) {
        entries_[i].hold();
    }
    head_.hold();
    last_.hold();

    // P1: Flush — invalidate matching entries
    for (int i = 0; i < LSQ_SIZE; i++) {
        if (exec.flush_mask[i].read()) {
            entries_[i].valid.next_raw() = 0;
        }
    }

    // P2: Commit invalidate (store committed)
    for (int i = 0; i < LSQ_SIZE; i++) {
        if (commit.invalidate_req[i].read()) {
            entries_[i].valid.next_raw() = 0;
        }
    }

    // P3: Issue push (suppressed when flush is active)
    bool adv_last = false;
    if (issue.push_valid.read() && !issue.suppressed.read()) {
        size_t tag = last_.cur();
        entries_[tag].valid.next_raw() = 1;
        entries_[tag].is_load.next_raw() = issue.push_is_load.read();
        entries_[tag].rob_tag.next_raw() = issue.push_rob_tag.read();
        entries_[tag].addr_ready.next_raw() = 0; entries_[tag].data_ready.next_raw() = 0;
        entries_[tag].prs2_or_prd.next_raw() = issue.push_prs2_or_prd.read();
        entries_[tag].data.next_raw() = 0;
        entries_[tag].width.next_raw() = issue.push_width.read();
        entries_[tag].is_unsigned.next_raw() = issue.push_is_unsigned.read();
        entries_[tag].mem_wait.next_raw() = 0;
        adv_last = true;
    }

    // Issue prs2_or_prd update (for loads after rename)
    if (pnum.valid.read()) {
        entries_[pnum.idx.read()].prs2_or_prd.next_raw() = pnum.val.read();
    }

    // P4: Execute — addr_ready + store data
    for (int i = 0; i < LSQ_SIZE; i++) {
        if (exec.set_addr_ready_req[i].read()) {
            entries_[i].addr_ready.next_raw() = 1;
            entries_[i].addr.next_raw() = exec.set_addr_val[i].read();
        }
        if (exec.set_store_data_req[i].read()) {
            entries_[i].data_ready.next_raw() = 1;
            entries_[i].data.next_raw() = exec.set_store_data_val[i].read();
        }
    }

    // P5: Memory — mem_wait + load latch + load_data + invalidate
    for (int i = 0; i < LSQ_SIZE; i++) {
        if (mem.set_mem_wait_req[i].read()) {
            entries_[i].mem_wait.next_raw() = mem.set_mem_wait_val[i].read();
        }
        if (mem.set_load_latch_req[i].read()) {
            entries_[i].data.next_raw() = mem.set_load_latch_val[i].read();
        }
        if (mem.set_load_data_req[i].read()) {
            entries_[i].data_ready.next_raw() = 1;
            entries_[i].data.next_raw() = mem.set_load_data_val[i].read();
            entries_[i].valid.next_raw() = 0;
        }
        if (mem.invalidate_req[i].read()) {
            entries_[i].valid.next_raw() = 0;
        }
    }

    // Last advance
    if (adv_last) {
        last_.next_raw() = static_cast<u32>((last_.cur() + 1) % LSQ_SIZE);
    }

    // P6: Commit set head
    if (commit.set_head_valid.read()) {
        head_.next_raw() = commit.set_head_val.read();
    }
}
