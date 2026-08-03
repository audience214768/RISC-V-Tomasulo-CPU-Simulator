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
        Register<1> pred_taken;
        Register<32> pred_target;
        void hold() {
            valid.hold(); ins_raw.hold(); ins_opcode.hold(); ins_func3.hold();
            ins_func7.hold(); ins_imm.hold(); rob_tag.hold(); lsq_tag.hold();
            prs1.hold(); prs2.hold(); prd.hold(); pc.hold();
            pred_taken.hold(); pred_target.hold();
        }
        void tick() {
            valid.tick(); ins_raw.tick(); ins_opcode.tick(); ins_func3.tick();
            ins_func7.tick(); ins_imm.tick(); rob_tag.tick(); lsq_tag.tick();
            prs1.tick(); prs2.tick(); prd.tick(); pc.tick();
            pred_taken.tick(); pred_target.tick();
        }
        void reset() {
            valid.reset(0); ins_raw.reset(0); ins_opcode.reset(0); ins_func3.reset(0);
            ins_func7.reset(0); ins_imm.reset(0); rob_tag.reset(0); lsq_tag.reset(0);
            prs1.reset(0); prs2.reset(0); prd.reset(0); pc.reset(0);
            pred_taken.reset(0); pred_target.reset(0);
        }
    };
    Entry entries_[RS_SIZE];

public:
    RSModule() { reset(); }

    void drive_read_ports(RSReadPorts &p) {
        bool any_free = false;
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
            p.pred_taken[i].write(entries_[i].pred_taken.cur());
            p.pred_target[i].write(entries_[i].pred_target.cur());
            if (!entries_[i].valid.cur()) any_free = true;
        }
        p.full.write(any_free ? 0 : 1);
    }

    void eval(
        const IssueRSWritePorts &issue,
        const ExecRSWritePorts &exec
    );

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
