#pragma once
#include "rtl/register.hpp"
#include "ports/lsq_ports.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <cstdio>
#include <cstdlib>

class LSQModule {
    struct Entry {
        Register<1> valid, is_load, addr_ready, data_ready, is_unsigned;
        Register<32> rob_tag, addr, prs2_or_prd, data, mem_wait;
        Register<8>  width;
        void hold() { 
            valid.hold(); is_load.hold(); rob_tag.hold(); addr_ready.hold(); addr.hold();
            data_ready.hold(); prs2_or_prd.hold(); data.hold(); width.hold();
            is_unsigned.hold(); mem_wait.hold(); 
        }
        void tick() { 
            valid.tick(); is_load.tick(); rob_tag.tick(); addr_ready.tick(); addr.tick();
            data_ready.tick(); prs2_or_prd.tick(); data.tick(); width.tick();
            is_unsigned.tick(); mem_wait.tick(); 
        }
        void reset() { 
            valid.reset(0); is_load.reset(0); rob_tag.reset(0); addr_ready.reset(0);
            addr.reset(0); data_ready.reset(0); prs2_or_prd.reset(0); data.reset(0);
            width.reset(0); is_unsigned.reset(0); mem_wait.reset(0); 
        }
    };
    Entry entries_[LSQ_SIZE];
    Register<32> head_, last_;

public:
    LSQModule() { reset(); }

    void drive_read_ports(LSQReadPorts &p) {
        p.head.write(head_.cur()); 
        p.last.write(last_.cur());
        p.full.write(((last_.cur()+1) % LSQ_SIZE == head_.cur())?1:0);
        for (int i = 0; i < LSQ_SIZE; i++) {
            p.valid[i].write(entries_[i].valid.cur()); p.is_load[i].write(entries_[i].is_load.cur());
            p.addr_ready[i].write(entries_[i].addr_ready.cur()); p.data_ready[i].write(entries_[i].data_ready.cur());
            p.addr[i].write(entries_[i].addr.cur()); p.data[i].write(entries_[i].data.cur());
            p.rob_tag[i].write(entries_[i].rob_tag.cur()); p.prs2_or_prd[i].write(entries_[i].prs2_or_prd.cur());
            p.width[i].write(entries_[i].width.cur()); p.is_unsigned[i].write(entries_[i].is_unsigned.cur());
            p.mem_wait[i].write(entries_[i].mem_wait.cur());
        }
    }

    void eval(const IssueLSQWritePorts &issue, const LSQPnumWritePorts &pnum, const ExecLSQWritePorts &exec,
              const MemLSQWritePorts &mem, const CommitLSQWritePorts &commit) {
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

        // P5: Memory — mem_wait + load_data + invalidate
        for (int i = 0; i < LSQ_SIZE; i++) {
            if (mem.set_mem_wait_req[i].read()) {
                entries_[i].mem_wait.next_raw() = mem.set_mem_wait_val[i].read();
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

    void tick() { 
        for (int i = 0; i < LSQ_SIZE; i++) {
            entries_[i].tick(); 
        }
        head_.tick(); 
        last_.tick(); 
    }
    void reset() { 
        for (int i = 0; i < LSQ_SIZE; i++) {
            entries_[i].reset(); 
        }
        head_.reset(0); 
        last_.reset(0); 
    }
};
