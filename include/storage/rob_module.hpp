#pragma once
#include "rtl/register.hpp"
#include "ports/rob_ports.hpp"
#include "utils/config.hpp"
#include <cstdio>

class ROBModule {
    struct Entry {
        Register<1> ready; Register<32> ins_raw, new_pnum, old_pnum, lsq_tag;
        Register<7> ins_opcode; Register<5> ins_rd;
        void hold() { 
            ready.hold(); ins_raw.hold(); ins_opcode.hold(); ins_rd.hold();
            new_pnum.hold(); old_pnum.hold(); lsq_tag.hold(); 
        }
        void tick() { 
            ready.tick(); ins_raw.tick(); ins_opcode.tick(); ins_rd.tick();
            new_pnum.tick(); old_pnum.tick(); lsq_tag.tick(); 
        }
        void reset() { 
            ready.reset(0); ins_raw.reset(0); ins_opcode.reset(0); ins_rd.reset(0);
            new_pnum.reset(0); old_pnum.reset(0); lsq_tag.reset(0); 
        }
    };
    Entry entries_[ROB_SIZE];
    Register<32> head_, last_;

public:
    ROBModule() { reset(); }

    void drive_read_ports(ROBReadPorts &p) {
        p.head.write(head_.cur()); 
        p.last.write(last_.cur());
        p.empty.write(head_.cur() == last_.cur() ? 1 : 0);
        p.full.write(((last_.cur() + 1) % ROB_SIZE == head_.cur()) ? 1 : 0);
        for (int i = 0; i < ROB_SIZE; i++) {
            p.ready[i].write(entries_[i].ready.cur()); 
            p.opcode[i].write(entries_[i].ins_opcode.cur());
            p.rd[i].write(entries_[i].ins_rd.cur()); 
            p.new_pnum[i].write(entries_[i].new_pnum.cur());
            p.old_pnum[i].write(entries_[i].old_pnum.cur()); 
            p.lsq_tag[i].write(entries_[i].lsq_tag.cur());
            p.ins_raw[i].write(entries_[i].ins_raw.cur());
        }
    }

    void eval(
        const IssueROBWritePorts &issue,
        const CommitROBWritePorts &commit,
        const FlushROBWritePorts &flush,
        const ReadyROBWritePorts &ready
    );

    void tick() { 
        for (int i = 0; i < ROB_SIZE; i++) {
            entries_[i].tick(); 
        }
        head_.tick(); 
        last_.tick(); 
    }
    void reset() { 
        for (int i = 0; i < ROB_SIZE; i++) {
            entries_[i].reset(); 
        }
        head_.reset(0); 
        last_.reset(0); 
    }
};
