#pragma once
#include "rtl/wire.hpp"
#include "utils/config.hpp"

struct CDBReadPorts { 
    Wire<1> valid[CDB_SIZE]; 
    Wire<32> prd[CDB_SIZE];
    Wire<32> result[CDB_SIZE], rob_tag[CDB_SIZE]; 
};

struct ExecCDBWritePorts {
    static constexpr int kMaxPush = 4;
    Wire<1> push_valid[kMaxPush]; 
    Wire<32> push_prd[kMaxPush], push_result[kMaxPush], push_rob_tag[kMaxPush];
    void clear(){
        wire_clear(push_valid);
        wire_clear(push_prd);
        wire_clear(push_result);
        wire_clear(push_rob_tag);
    }
};
struct MemCDBWritePorts {
    static constexpr int kMaxPush = LSQ_SIZE;
    Wire<1> push_valid[kMaxPush];
    Wire<32> push_prd[kMaxPush], push_result[kMaxPush], push_rob_tag[kMaxPush];
    void clear(){
        wire_clear(push_valid);
        wire_clear(push_prd);
        wire_clear(push_result);
        wire_clear(push_rob_tag);
    }
};
struct WBCDBWritePorts {
    Wire<1> clear_req[CDB_SIZE];
    void clear(){ wire_clear(clear_req); }
};
