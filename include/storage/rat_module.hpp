#pragma once
#include "rtl/register.hpp"
#include "ports/rat_ports.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"

class RATModule {
public:
    RATModule() { reset(); }

    void drive_read_ports(RATReadPorts &p) {
        for (int i = 0; i < NUM_ARCH_REGS; i++) {
            p.map[i].write(map_[i].cur());
        }
    }

    void eval(const IssueRATWritePorts &issue, const FlushRATWritePorts &flush) {
        bool written[NUM_ARCH_REGS] = {false};
        for (int i = 0; i < NUM_ARCH_REGS; i++) {
            map_[i].hold();
        }

        // Priority: Flush restore > Issue rename
        u8 fc = static_cast<u8>(flush.restore_count.read());
        for (u8 i = 0; i < fc; i++) {
            ArchRegNum rd = static_cast<ArchRegNum>(flush.restore_rd[i].read());
            ///if (rd == 5) fprintf(stderr, "change the t0 from %d to %d\n", map_[rd].next_raw(), flush.restore_old[i].read());
            map_[rd].next_raw() = flush.restore_old[i].read();
            written[rd] = true;
        }
        if (issue.rename_valid.read()) {
            ArchRegNum rd = static_cast<ArchRegNum>(issue.rename_rd.read());
            //fprintf(stderr, "old = %d new = %d\n", map_[rd].cur(), issue.rename_new.read());
            if (!written[rd]) {
                map_[rd].next_raw() = issue.rename_new.read();
            }
        }
    }

    void tick() { 
        for (int i = 0; i < NUM_ARCH_REGS; i++) {
            map_[i].tick(); 
        }
    }
    void reset() { 
        for (int i = 0; i < NUM_ARCH_REGS; i++) {
            map_[i].reset(i); 
        }
    }

private:
    Register<32> map_[NUM_ARCH_REGS];
};
