#include "storage/rat_module.hpp"

void RATModule::eval(
    const IssueRATWritePorts &issue,
    const FlushRATWritePorts &flush
) {
    bool written[NUM_ARCH_REGS] = {false};
    for (int i = 0; i < NUM_ARCH_REGS; i++) {
        map_[i].hold();
    }

    // Priority: Flush restore > Issue rename
    u8 fc = static_cast<u8>(flush.restore_count.read());
    for (u8 i = 0; i < fc; i++) {
        ArchRegNum rd = static_cast<ArchRegNum>(flush.restore_rd[i].read());
        map_[rd].next_raw() = flush.restore_old[i].read();
        written[rd] = true;
    }
    if (issue.rename_valid.read() && !issue.suppressed.read()) {
        ArchRegNum rd = static_cast<ArchRegNum>(issue.rename_rd.read());
        if (!written[rd]) {
            map_[rd].next_raw() = issue.rename_new.read();
        }
    }
}
