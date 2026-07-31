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

    void eval(const IssueROBWritePorts &issue, const CommitROBWritePorts &commit,
              const FlushROBWritePorts &flush, const ReadyROBWritePorts &ready) {
        for (int i = 0; i < ROB_SIZE; i++) {
            entries_[i].hold();
        }
        head_.hold(); 
        last_.hold();

        // P1 (highest): Flush — resets last, overrides everything below
        if (flush.set_last_valid.read()) {
            last_.next_raw() = flush.set_last_val.read();
        }
        // P2: Commit — sets head
        if (commit.set_head_valid.read()) {
            head_.next_raw() = commit.set_head_val.read();
        }
        // P3: Issue — push new entry (only if no flush)
        if (issue.push_valid.read() && !flush.set_last_valid.read()) {
            size_t tag = last_.cur();
            entries_[tag].ready.next_raw() = 0;
            entries_[tag].ins_raw.next_raw() = issue.push_ins_raw.read();
            entries_[tag].ins_opcode.next_raw() = issue.push_opcode.read();
            entries_[tag].ins_rd.next_raw() = issue.push_rd.read();
            entries_[tag].new_pnum.next_raw() = issue.push_new.read();
            entries_[tag].old_pnum.next_raw() = issue.push_old.read();
            entries_[tag].lsq_tag.next_raw() = issue.push_lsq.read();
            // Only advance last if flush didn't override it
            if (!flush.set_last_valid.read()) {
                last_.next_raw() = static_cast<u32>((tag + 1) % ROB_SIZE);
            }
        }
        // P4: Set ready (independent, multiple writers per entry OK — all set to 1)
        for (int i = 0; i < ROB_SIZE; i++) {
            if (ready.set_ready_req[i].read()) {
                entries_[i].ready.next_raw() = 1;
                //fprintf(stderr, "Rob release %d\n", i);
            }
        }
        //fprintf(stderr, "%0x\n", entries_[27].ins_raw.next_raw());
        //fprintf(stderr, "head = %u last = %u\n", head_.next_raw(), last_.next_raw());
    }

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
