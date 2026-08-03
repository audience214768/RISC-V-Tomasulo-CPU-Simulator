#include "storage/rs_module.hpp"
#include <cstdio>
#include <cstdlib>

void RSModule::eval(
    const IssueRSWritePorts &issue,
    const ExecRSWritePorts &exec
) {
    for (int i = 0; i < RS_SIZE; i++) {
        entries_[i].hold();
    }

    // Priority 1: Flush (invalidate entries)
    for (int i = 0; i < RS_SIZE; i++) {
        if (exec.flush_mask[i].read()) {
            entries_[i].valid.next_raw() = 0;
        }
    }

    // Priority 2: Clear (remove executed entries)
    u8 cc = static_cast<u8>(exec.clear_count.read());
    for (u8 i = 0; i < cc && i < ExecRSWritePorts::kMaxClear; i++) {
        entries_[exec.clear_idx[i].read()].valid.next_raw() = 0;
    }

    // Priority 3: Push (issue new entry, suppressed when flush is active)
    if (issue.push_valid.read() && !issue.suppressed.read()) {
        int slot = -1;
        for (int i = 0; i < RS_SIZE; i++) {
            if (!entries_[i].valid.cur() && entries_[i].valid.next_raw() == 0) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            fprintf(stderr, "RS: push on full!\n");
            exit(1);
        }
        auto &e = entries_[slot];
        e.valid.next_raw() = 1; e.ins_raw.next_raw() = issue.push_ins_raw.read();
        e.ins_opcode.next_raw() = issue.push_opcode.read(); e.ins_func3.next_raw() = issue.push_ins_func3.read();
        e.ins_func7.next_raw() = issue.push_ins_func7.read(); e.ins_imm.next_raw() = issue.push_ins_imm.read();
        e.rob_tag.next_raw() = issue.push_rob_tag.read(); e.lsq_tag.next_raw() = issue.push_lsq_tag.read();
        e.prs1.next_raw() = issue.push_prs1.read(); e.prs2.next_raw() = issue.push_prs2.read();
        e.prd.next_raw() = issue.push_prd.read(); e.pc.next_raw() = issue.push_pc.read();
        e.pred_taken.next_raw() = issue.push_pred_taken.read();
        e.pred_target.next_raw() = issue.push_pred_target.read();
    }
}
