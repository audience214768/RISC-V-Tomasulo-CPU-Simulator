#pragma once
#include "rtl/wire.hpp"
#include "utils/config.hpp"

struct LSQReadPorts {
    Wire<32> head, last; Wire<1> full;
    Wire<1> valid[LSQ_SIZE], is_load[LSQ_SIZE], addr_ready[LSQ_SIZE], data_ready[LSQ_SIZE], is_unsigned[LSQ_SIZE];
    Wire<32> addr[LSQ_SIZE], data[LSQ_SIZE], rob_tag[LSQ_SIZE], prs2_or_prd[LSQ_SIZE], mem_wait[LSQ_SIZE];
    Wire<8> width[LSQ_SIZE];
};

struct IssueLSQWritePorts {
    Wire<1> push_valid, push_is_load, push_is_unsigned;
    Wire<32> push_rob_tag, push_prs2_or_prd; Wire<8> push_width;
    void clear(){ push_valid.write(0); }
};
struct LSQPnumWritePorts {
     Wire<1> valid; 
     Wire<8> idx; 
     Wire<32> val; 
     void clear(){ valid.write(0); } 
};
struct ExecLSQWritePorts {
    Wire<1> set_addr_ready_req[LSQ_SIZE]; Wire<32> set_addr_val[LSQ_SIZE];
    Wire<1> set_store_data_req[LSQ_SIZE]; Wire<32> set_store_data_val[LSQ_SIZE];
    Wire<1> flush_mask[LSQ_SIZE];
    void clear(){
        wire_clear(set_addr_ready_req);
        wire_clear(set_store_data_req);
        wire_clear(flush_mask);
    }
};
struct MemLSQWritePorts {
    Wire<1> set_mem_wait_req[LSQ_SIZE]; Wire<32> set_mem_wait_val[LSQ_SIZE];
    Wire<1> set_load_data_req[LSQ_SIZE]; Wire<32> set_load_data_val[LSQ_SIZE];
    Wire<1> invalidate_req[LSQ_SIZE];
    void clear(){
        wire_clear(set_mem_wait_req);
        wire_clear(set_load_data_req);
        wire_clear(invalidate_req);
    }
};
struct CommitLSQWritePorts {
    Wire<1> invalidate_req[LSQ_SIZE]; Wire<1> set_head_valid; Wire<32> set_head_val;
    void clear(){
        wire_clear(invalidate_req);
        set_head_valid.write(0);
    }
};
