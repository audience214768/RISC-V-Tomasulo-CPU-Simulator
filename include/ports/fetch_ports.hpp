#pragma once
#include "rtl/wire.hpp"
#include "utils/config.hpp"

struct FetchReadPorts {
    Wire<32> pc;
    Wire<32> f2i_raw;
    Wire<32> f2i_pc;
    Wire<1>  f2i_valid;
    Wire<1>  f2i_pred;        // prediction made at fetch, rides with the inst
    Wire<32> f2i_pred_target; // JALR return target (RAS top); else unused
    Wire<1>  halt;
};

struct HaltRequestWritePorts {
    Wire<1> req;
    void clear() { req.write(0); }
};

struct ExecToFetchWritePorts {
    Wire<1>  mispredict;
    Wire<32> correct_pc;
    void clear() { mispredict.write(0); }
};

struct IssueToFetchWritePorts {
    Wire<1>  stall;
    void clear() { stall.write(0); }
};
