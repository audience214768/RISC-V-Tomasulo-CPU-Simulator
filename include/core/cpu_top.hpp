#pragma once

#include "storage/prf_module.hpp"
#include "storage/ready_table_module.hpp"
#include "storage/rat_module.hpp"
#include "storage/free_list_module.hpp"
#include "storage/cdb_module.hpp"
#include "storage/rs_module.hpp"
#include "storage/rob_module.hpp"
#include "storage/lsq_module.hpp"
#include "storage/fetch_module.hpp"
#include "storage/bht_module.hpp"
#include "storage/ras_module.hpp"

#include "ports/prf_ports.hpp"
#include "ports/mem_ports.hpp"
#include "ports/ready_table_ports.hpp"
#include "ports/rat_ports.hpp"
#include "ports/free_list_ports.hpp"
#include "ports/cdb_ports.hpp"
#include "ports/rs_ports.hpp"
#include "ports/rob_ports.hpp"
#include "ports/lsq_ports.hpp"
#include "ports/fetch_ports.hpp"
#include "ports/bht_ports.hpp"
#include "ports/ras_ports.hpp"

#include "utils/config.hpp"
#include "utils/types.hpp"

#include <cstddef>
#include <cstring>

class TomasuloTop {
public:
    TomasuloTop();
    void run();

private:
    void tick();
    void eval_commit();
    void eval_writeback();
    void eval_memory();
    void eval_issue();
    void eval_execute();

    static auto decode(u32 raw) -> Instruction;
    static auto sign_extend(u32 val, u8 bits) -> u32;
    static auto ALU_R(u32 f3, u32 f7, u32 r1, u32 r2) -> u32;
    static auto ALU_I(u32 f3, u32 f7, u32 r1, u32 imm) -> u32;
    static auto branch_cond(u32 f3, u32 r1, u32 r2) -> bool;
    void flush_pipeline(size_t branch_rob_tag);
    void load_memory();

    // ---- storage modules ----
    PRFModule        prf_;
    ReadyTableModule ready_table_;
    RATModule        rat_;
    FreeListModule   free_list_;
    CDBModule        cdb_;
    RSModule         rs_;
    ROBModule        rob_;
    LSQModule        lsq_;
    FetchModule      fetch_;
    BHTModule        bht_;
    RASModule        ras_;

    // ---- read port wires ----
    PRFReadPorts        prf_rp_;
    ReadyTableReadPorts rt_rp_;
    RATReadPorts        rat_rp_;
    FreeListReadPorts   fl_rp_;
    CDBReadPorts        cdb_rp_;
    RSReadPorts         rs_rp_;
    ROBReadPorts        rob_rp_;
    LSQReadPorts        lsq_rp_;
    FetchReadPorts      fetch_rp_;
    BHTReadPorts        bht_rp_;
    RASReadPorts        ras_rp_;

    // ---- write port wires (per-writer bundles) ----
    WBPRFWritePorts       wb_prf_;
    WBReadyWritePorts     wb_ready_;
    IssueReadyWritePorts  issue_ready_;
    FlushReadyWritePorts  flush_ready_;
    IssueRATWritePorts    issue_rat_;
    FlushRATWritePorts    flush_rat_;
    IssueFLWritePorts     issue_fl_;
    CommitFLWritePorts    commit_fl_;
    FlushFLWritePorts     flush_fl_;
    IssueROBWritePorts    issue_rob_;
    CommitROBWritePorts   commit_rob_;
    FlushROBWritePorts    flush_rob_;
    ReadyROBWritePorts    ready_rob_;
    IssueRSWritePorts     issue_rs_;
    ExecRSWritePorts      exec_rs_;
    IssueLSQWritePorts    issue_lsq_;
    LSQPnumWritePorts     lsq_pnum_;
    ExecLSQWritePorts     exec_lsq_;
    MemLSQWritePorts      mem_lsq_;
    CommitLSQWritePorts   commit_lsq_;
    ExecCDBWritePorts     exec_cdb_;
    MemCDBWritePorts      mem_cdb_;
    WBCDBWritePorts       wb_cdb_;

    // ---- Fetch write ports ----
    HaltRequestWritePorts  halt_req_;
    ExecToFetchWritePorts  exec_to_fetch_;
    IssueToFetchWritePorts issue_to_fetch_;
    ExecBHTWritePorts     bht_exec_;
    FetchRASWritePorts    ras_fetch_;
    FlushRASWritePorts    ras_restore_;

    // ---- memory write port (store commit, applied at the clock edge) ----
    MemWritePorts mem_wr_;

    // ---- external memory ----
    MemState mem_;

    // ---- stats ----
    size_t clock_           = 0;
    size_t branch_count_    = 0;
    size_t mispredict_count_ = 0;
    size_t flush_count_      = 0;
    size_t jalr_mispredict_count_ = 0;
    size_t flush_rs_stall_ = 0, flush_lsq_stall_ = 0, flush_rob_stall_ = 0, flush_fl_stall_ = 0;
};
