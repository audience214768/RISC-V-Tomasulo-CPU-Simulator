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

    void eval(
        const ExecCDBWritePorts &exec,
        const MemCDBWritePorts &mem,
        const WBCDBWritePorts &wb
    );

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
