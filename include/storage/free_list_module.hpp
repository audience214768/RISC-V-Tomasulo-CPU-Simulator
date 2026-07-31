#pragma once
#include "rtl/register.hpp"
#include "ports/free_list_ports.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <cstdio>
#include <cstdlib>

class FreeListModule {
public:
    FreeListModule() { reset(); }

    void drive_read_ports(FreeListReadPorts &p) {
        p.empty.write(count_.cur() == 0 ? 1 : 0);
        p.head_val.write(count_.cur() > 0 ? buf_[head_.cur()].cur() : 0);
    }

    void eval(const IssueFLWritePorts &issue, const CommitFLWritePorts &commit, const FlushFLWritePorts &flush) {
        size_t nxt_head = head_.cur(), nxt_tail = tail_.cur(), nxt_count = count_.cur();
        for (int i = 0; i < NUM_PHYS_REGS; i++) {
            buf_[i].hold();
        }
        head_.hold(); 
        tail_.hold(); 
        count_.hold();

        // Priority: Flush pushes > Commit pushes > Issue pop
        for (u8 i = 0; i < static_cast<u8>(flush.push_count.read()); i++) {
            buf_[nxt_tail].next_raw() = flush.push_pregs[i].read();
            nxt_tail = (nxt_tail + 1) % NUM_PHYS_REGS; 
            nxt_count++;
        }
        for (u8 i = 0; i < static_cast<u8>(commit.push_count.read()); i++) {
            buf_[nxt_tail].next_raw() = commit.push_pregs[i].read();
            nxt_tail = (nxt_tail + 1) % NUM_PHYS_REGS; 
            nxt_count++;
        }
        if (issue.pop_req.read() && !issue.suppressed.read()) {
            if (nxt_count == 0) {
                fprintf(stderr, "FreeList: pop on empty!\n");
                exit(1);
            }
            nxt_head = (nxt_head + 1) % NUM_PHYS_REGS; nxt_count--;
        }

        head_.next_raw() = static_cast<u32>(nxt_head);
        tail_.next_raw() = static_cast<u32>(nxt_tail);
        count_.next_raw() = static_cast<u32>(nxt_count);
        // for (int i = head_.next_raw(); i != tail_.next_raw(); i = (i + 1) % NUM_PHYS_REGS) {
        //     fprintf(stderr, "%d ", buf_[i].next_raw());
        // }
        // fprintf(stderr, "\n");
    }

    void tick() {
        for (int i = 0; i < NUM_PHYS_REGS; i++) {
            buf_[i].tick();
        }
        head_.tick();
        tail_.tick();
        count_.tick();
    }
    void reset() {
        for (int i = 0; i < NUM_PHYS_REGS; i++) {
            buf_[i].reset(0);
        }
        head_.reset(0); 
        tail_.reset(0); 
        count_.reset(0);
        size_t t = 0;
        for (u32 i = NUM_ARCH_REGS; i < NUM_PHYS_REGS; i++) { 
            buf_[t].reset(i); 
            t++; 
        }
        tail_.reset(static_cast<u32>(t)); 
        count_.reset(static_cast<u32>(t));
    }

private:
    Register<32> buf_[NUM_PHYS_REGS], head_, tail_, count_;
};
