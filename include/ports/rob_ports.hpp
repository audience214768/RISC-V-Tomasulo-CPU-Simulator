#pragma once
#include "rtl/wire.hpp"
#include "utils/config.hpp"

struct ROBReadPorts {
    Wire<32> head, last; 
    Wire<1> empty, full;
    Wire<1> ready[ROB_SIZE]; 
    Wire<7> opcode[ROB_SIZE]; 
    Wire<5> rd[ROB_SIZE];
    Wire<32> new_pnum[ROB_SIZE], old_pnum[ROB_SIZE], lsq_tag[ROB_SIZE], ins_raw[ROB_SIZE];
};

struct IssueROBWritePorts {
    Wire<1> push_valid; 
    Wire<7> push_opcode; 
    Wire<5> push_rd;
    Wire<32> push_new, push_old, push_lsq, push_ins_raw;
    void clear(){ push_valid.write(0); }
};
struct CommitROBWritePorts {
    Wire<1> set_head_valid; 
    Wire<32> set_head_val;
    void clear(){ set_head_valid.write(0); }
};
struct FlushROBWritePorts {
    Wire<1> set_last_valid; 
    Wire<32> set_last_val;
    void clear(){ set_last_valid.write(0); }
};
struct ReadyROBWritePorts {
    Wire<1> set_ready_req[ROB_SIZE];
    void clear(){ wire_clear(set_ready_req); }
};
