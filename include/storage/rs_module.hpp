#pragma once
#include "rtl/register.hpp"
#include "ports/rs_ports.hpp"
#include "utils/config.hpp"
#include <cstdio>
#include <cstdlib>

class RSModule {
    struct Entry {
        Register<1> valid; 
        Register<32> ins_raw, ins_imm, rob_tag, lsq_tag, prs1, prs2, prd, pc;
        Register<7> ins_opcode; 
        Register<3> ins_func3; 
        Register<7> ins_func7;
        void hold() { 
            valid.hold(); ins_raw.hold(); ins_opcode.hold(); ins_func3.hold();
            ins_func7.hold(); ins_imm.hold(); rob_tag.hold(); lsq_tag.hold();
            prs1.hold(); prs2.hold(); prd.hold(); pc.hold(); 
        }
        void tick() { 
            valid.tick(); ins_raw.tick(); ins_opcode.tick(); ins_func3.tick();
            ins_func7.tick(); ins_imm.tick(); rob_tag.tick(); lsq_tag.tick();
            prs1.tick(); prs2.tick(); prd.tick(); pc.tick(); 
        }
        void reset() { 
            valid.reset(0); ins_raw.reset(0); ins_opcode.reset(0); ins_func3.reset(0);
            ins_func7.reset(0); ins_imm.reset(0); rob_tag.reset(0); lsq_tag.reset(0);
            prs1.reset(0); prs2.reset(0); prd.reset(0); pc.reset(0); 
        }
    };
    Entry entries_[RS_SIZE];

public:
    RSModule() { reset(); }

    void drive_read_ports(RSReadPorts &p) {
        for (int i = 0; i < RS_SIZE; i++) {
            p.valid[i].write(entries_[i].valid.cur()); 
            p.opcode[i].write(entries_[i].ins_opcode.cur());
            p.ins_func3[i].write(entries_[i].ins_func3.cur()); 
            p.ins_func7[i].write(entries_[i].ins_func7.cur());
            p.prs1[i].write(entries_[i].prs1.cur()); 
            p.prs2[i].write(entries_[i].prs2.cur());
            p.prd[i].write(entries_[i].prd.cur()); 
            p.pc[i].write(entries_[i].pc.cur());
            p.rob_tag[i].write(entries_[i].rob_tag.cur()); 
            p.lsq_tag[i].write(entries_[i].lsq_tag.cur());
            p.ins_imm[i].write(entries_[i].ins_imm.cur());
        }
        bool f = true; 
        for (int i = 0; i < RS_SIZE; i++) {
            if (!entries_[i].valid.cur()) { 
                f = false; 
                break; 
            }
        }
        p.full.write(f ? 1 : 0);
    }

    void eval(const IssueRSWritePorts &issue, const ExecRSWritePorts &exec) {
        for (int i = 0; i < RS_SIZE; i++) {
            entries_[i].hold();
        }

        // Priority 1: Flush (invalidate entries)
        for (int i = 0; i < RS_SIZE; i++) {
            if (exec.flush_mask[i].read()) {
                //fprintf(stderr, "flush invalidate %d\n", i);
                entries_[i].valid.next_raw() = 0;
            }
        }

        // Priority 2: Clear (remove executed entries)
        u8 cc = static_cast<u8>(exec.clear_count.read());
        for (u8 i = 0; i < cc && i < ExecRSWritePorts::kMaxClear; i++) {
            //fprintf(stderr, "exe invalidate %d\n", exec.clear_idx[i].read());
            entries_[exec.clear_idx[i].read()].valid.next_raw() = 0;
        }

        // Priority 3: Push (issue new entry)
        if (issue.push_valid.read()) {
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
        }
        // for (int i = 0; i < RS_SIZE; i++) {
        //     if (entries_[i].valid.next_raw()) fprintf(stderr, "RS: %0x %d %d %d %d\n", entries_[i].pc.next_raw(), entries_[i].prs1.next_raw(), entries_[i].prs2.next_raw(), entries_[i].prd.next_raw(), entries_[i].rob_tag.next_raw());
        // }
        // fprintf(stderr, "\n");
    }

    void tick() { 
        for (int i = 0; i < RS_SIZE; i++) {
            entries_[i].tick(); 
        }
    }
    void reset() { 
        for (int i = 0; i < RS_SIZE; i++) {
            entries_[i].reset(); 
        }
    }
};
