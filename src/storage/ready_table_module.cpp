#include "storage/ready_table_module.hpp"

void ReadyTableModule::eval(
    const WBReadyWritePorts &wb,
    const IssueReadyWritePorts &issue,
    const FlushReadyWritePorts &flush
) {
    for (int i = 0; i < NUM_PHYS_REGS; i++) {
        bits_[i].hold();
    }
    // Priority: Flush > Issue > WriteBack (non-overlapping pregs)
    for (int i = 0; i < NUM_PHYS_REGS; i++) {
        if (flush.clear_req[i].read()) {
            bits_[i].next_raw() = 0;
        } else if (issue.clear_req[i].read() && !issue.suppressed.read()) {
            bits_[i].next_raw() = 0;
        } else if (wb.set_req[i].read()) {
            bits_[i].next_raw() = 1;
        }
    }
}
