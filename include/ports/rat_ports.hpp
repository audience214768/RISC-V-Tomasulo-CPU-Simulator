#pragma once
#include "rtl/wire.hpp"
#include "utils/config.hpp"

struct RATReadPorts { 
    Wire<32> map[NUM_ARCH_REGS]; 
};

struct IssueRATWritePorts {
    Wire<1> rename_valid;
    Wire<5> rename_rd;
    Wire<32> rename_new;
    Wire<1> suppressed;
    void clear(){
        rename_valid.write(0);
        rename_rd.write(0);
        rename_new.write(0);
        suppressed.write(0);
    }
};
struct FlushRATWritePorts {
    static constexpr int kMaxRestore = 32;
    Wire<5> restore_rd[kMaxRestore];
    Wire<32> restore_old[kMaxRestore];
    Wire<8> restore_count;
    void clear(){
        wire_clear(restore_rd);
        wire_clear(restore_old);
        restore_count.write(0);
    }
};
