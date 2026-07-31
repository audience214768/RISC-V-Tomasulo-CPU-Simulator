#pragma once
#include "rtl/wire.hpp"
#include "utils/config.hpp"

struct ReadyTableReadPorts { 
    Wire<1> ready[NUM_PHYS_REGS]; 
};

struct WBReadyWritePorts { 
    Wire<1> set_req[NUM_PHYS_REGS];   
    void clear(){ wire_clear(set_req); } 
};
struct IssueReadyWritePorts { 
    Wire<1> clear_req[NUM_PHYS_REGS]; 
    void clear(){ wire_clear(clear_req); } 
};
struct FlushReadyWritePorts { 
    Wire<1> clear_req[NUM_PHYS_REGS]; 
    void clear(){ wire_clear(clear_req); } 
};
