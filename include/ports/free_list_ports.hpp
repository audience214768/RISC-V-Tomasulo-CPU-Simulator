#pragma once
#include "rtl/wire.hpp"
#include "utils/config.hpp"

struct FreeListReadPorts {
    Wire<1> empty;
    Wire<32> head_val;
};

struct IssueFLWritePorts  {
    Wire<1> pop_req;
    Wire<1> suppressed;
    void clear(){ pop_req.write(0); suppressed.write(0); }
};
struct CommitFLWritePorts {
    static constexpr int kMaxPush = 16;
    Wire<32> push_pregs[kMaxPush];
    Wire<8> push_count;
    void clear(){
        wire_clear(push_pregs);
        push_count.write(0);
    }
};
struct FlushFLWritePorts  {
    static constexpr int kMaxPush = ROB_SIZE;
    Wire<32> push_pregs[kMaxPush];
    Wire<8> push_count;
    void clear(){
        wire_clear(push_pregs);
        push_count.write(0);
    }
};
