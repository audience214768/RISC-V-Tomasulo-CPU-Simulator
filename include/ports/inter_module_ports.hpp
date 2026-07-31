#pragma once
#include "rtl/wire.hpp"

struct InterModulePorts {
    // Execute → Fetch/Issue
    Wire<1>  exec_mispredict;
    Wire<32> exec_correct_pc;

    // Issue → Fetch
    Wire<1>  issue_halt;
    Wire<1>  issue_pred_taken;
    Wire<32> issue_pred_target;
    Wire<1>  issue_stall;

    // Fetch outputs (driven by eval_fetch, read by eval_issue)
    Wire<32> fetch_pc;
    Wire<32> fetch_instruction_pc;
    Wire<32> fetch_raw_instruction;
};
