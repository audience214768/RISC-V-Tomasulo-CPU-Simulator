#pragma once
#include "rtl/wire.hpp"
#include "utils/config.hpp"

struct FreeListReadPorts { 
    Wire<1> empty; 
    Wire<32> head_val; 
};

struct IssueFLWritePorts  { 
    Wire<1> pop_req; 
    void clear(){ pop_req.write(0); } 
};
struct CommitFLWritePorts { 
    Wire<32> push_pregs[NUM_PHYS_REGS]; 
    Wire<8> push_count;
    void clear(){ 
        wire_clear(push_pregs);
        push_count.write(0);
    } 
};
struct FlushFLWritePorts  { 
    Wire<32> push_pregs[NUM_PHYS_REGS]; 
    Wire<8> push_count;
    void clear(){
        wire_clear(push_pregs);
        push_count.write(0);
    } 
};
