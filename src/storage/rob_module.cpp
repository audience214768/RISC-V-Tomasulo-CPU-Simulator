#include "storage/rob_module.hpp"

void ROBModule::eval(
    const IssueROBWritePorts &issue,
    const CommitROBWritePorts &commit,
    const FlushROBWritePorts &flush,
    const ReadyROBWritePorts &ready
) {
    for (int i = 0; i < ROB_SIZE; i++) {
        entries_[i].hold();
    }
    head_.hold();
    last_.hold();

    // P1 (highest): Flush — resets last, overrides everything below
    if (flush.set_last_valid.read()) {
        last_.next_raw() = flush.set_last_val.read();
        u32 fs = flush.set_last_val.read();
        u32 wlen = (last_.cur() + ROB_SIZE - fs) % ROB_SIZE;
        u32 hoff = (head_.cur() + ROB_SIZE - fs) % ROB_SIZE;
        if (hoff < wlen) {
            head_.next_raw() = fs;
        }
    }
    // P2: Commit — sets head
    if (commit.set_head_valid.read()) {
        head_.next_raw() = commit.set_head_val.read();
    }
    // P3: Issue — push new entry (only if no flush / walker suppression)
    if (issue.push_valid.read() && !flush.set_last_valid.read() && !issue.suppressed.read()) {
        size_t tag = last_.cur();
        entries_[tag].ready.next_raw() = 0;
        entries_[tag].ins_raw.next_raw() = issue.push_ins_raw.read();
        entries_[tag].ins_opcode.next_raw() = issue.push_opcode.read();
        entries_[tag].ins_rd.next_raw() = issue.push_rd.read();
        entries_[tag].new_pnum.next_raw() = issue.push_new.read();
        entries_[tag].old_pnum.next_raw() = issue.push_old.read();
        entries_[tag].lsq_tag.next_raw() = issue.push_lsq.read();
        if (!flush.set_last_valid.read()) {
            last_.next_raw() = static_cast<u32>((tag + 1) % ROB_SIZE);
        }
    }
    // P4: Set ready (independent, multiple writers per entry OK)
    for (int i = 0; i < ROB_SIZE; i++) {
        if (ready.set_ready_req[i].read()) {
            entries_[i].ready.next_raw() = 1;
        }
    }
}
