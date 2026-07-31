#pragma once
#include "rtl/register.hpp"
#include "ports/cdb_ports.hpp"
#include "utils/config.hpp"
#include <cstdio>
#include <cstdlib>

class CDBModule {
    struct Entry { Register<1> valid; Register<32> prd, result, rob_tag;
        void hold() { 
            valid.hold(); 
            prd.hold(); 
            result.hold(); 
            rob_tag.hold(); 
        }
        void tick() { 
            valid.tick(); 
            prd.tick(); 
            result.tick(); 
            rob_tag.tick(); 
        }
        void reset() { 
            valid.reset(0); 
            prd.reset(0); 
            result.reset(0); 
            rob_tag.reset(0); 
        }
    };
    Entry entries_[CDB_SIZE];

public:
    CDBModule() { reset(); }

    void drive_read_ports(CDBReadPorts &p) {
        for (int i = 0; i < CDB_SIZE; i++) {
            p.valid[i].write(entries_[i].valid.cur()); 
            p.prd[i].write(entries_[i].prd.cur());
            p.result[i].write(entries_[i].result.cur()); 
            p.rob_tag[i].write(entries_[i].rob_tag.cur());
        }
    }

    void eval(const ExecCDBWritePorts &exec, const MemCDBWritePorts &mem, const WBCDBWritePorts &wb) {
        for (int i = 0; i < CDB_SIZE; i++) {
            entries_[i].hold();
        }

        // Priority: Clear > Push (so cleared slots can be reused same cycle)
        for (int i = 0; i < CDB_SIZE; i++) {
            if (wb.clear_req[i].read()) {
                entries_[i].valid.next_raw() = 0;
            }
        }

        // Push: Execute (up to 4) + Memory (1)
        auto do_push = [&](u32 prd, u32 result, u32 rob_tag) {
            for (int i = 0; i < CDB_SIZE; i++) {
                if (!entries_[i].valid.cur() && entries_[i].valid.next_raw() == 0) {
                    entries_[i].valid.next_raw() = 1; 
                    entries_[i].prd.next_raw() = prd;
                    entries_[i].result.next_raw() = result; 
                    entries_[i].rob_tag.next_raw() = rob_tag;
                    //if (prd == 123) fprintf(stderr, "[CDB push] rob_tag=%d prd=%u res=0x%x\n", rob_tag, prd, result);
                    return;
                }
            }
            fprintf(stderr, "CDB: push on full!\n"); exit(1);
        };
        for (int j = 0; j < ExecCDBWritePorts::kMaxPush; j++) {
            if (exec.push_valid[j].read()) {
                do_push(exec.push_prd[j].read(), exec.push_result[j].read(), exec.push_rob_tag[j].read());
            }
        }
        if (mem.push_valid.read()) {
            do_push(mem.push_prd.read(), mem.push_result.read(), mem.push_rob_tag.read());
        }
    }

    void tick() { 
        for (int i = 0; i < CDB_SIZE; i++) {
            entries_[i].tick(); 
        }
    }
    void reset() { 
        for (int i = 0; i < CDB_SIZE; i++) {
            entries_[i].reset(); 
        }
    }
};
