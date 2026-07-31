#pragma once
#include "rtl/wire.hpp"
#include "utils/config.hpp"

struct RSReadPorts {
    Wire<1> valid[RS_SIZE], full; 
    Wire<7> opcode[RS_SIZE]; 
    Wire<3> ins_func3[RS_SIZE]; 
    Wire<7> ins_func7[RS_SIZE];
    Wire<32> prs1[RS_SIZE], prs2[RS_SIZE], prd[RS_SIZE], pc[RS_SIZE];
    Wire<32> rob_tag[RS_SIZE], lsq_tag[RS_SIZE], ins_imm[RS_SIZE];
};

struct IssueRSWritePorts {
    Wire<1> push_valid; 
    Wire<7> push_opcode; 
    Wire<3> push_ins_func3; 
    Wire<7> push_ins_func7;
    Wire<32> push_prs1, push_prs2, push_prd, push_pc, push_rob_tag, push_lsq_tag, push_ins_raw, push_ins_imm;
    void clear(){ push_valid.write(0); }
};
struct ExecRSWritePorts {
    static constexpr int kMaxClear = 2;
    Wire<8> clear_count; 
    Wire<8> clear_idx[kMaxClear]; 
    Wire<1> flush_mask[RS_SIZE];
    void clear(){
        clear_count.write(0);
        wire_clear(clear_idx);
        wire_clear(flush_mask);
    }
};
