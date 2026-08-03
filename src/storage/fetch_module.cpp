#include "storage/fetch_module.hpp"
#include <bit>
#include <array>

void FetchModule::eval(
    const HaltRequestWritePorts &halt_req,
    const ExecToFetchWritePorts &exec,
    const IssueToFetchWritePorts &issue,
    const MemState &mem
) {
    pc_.hold();
    f2i_raw_.hold();
    f2i_pc_.hold();
    f2i_valid_.hold();
    halt_.hold();

    if (halt_req.req.read()) {
        halt_.write(1);
    }
    if (halt_.cur()) {
        return;
    }

    if (exec.mispredict.read()) {
        u32 addr = exec.correct_pc.read();
        u32 raw  = read_instruction(mem, addr);
        f2i_raw_.write(raw);
        f2i_pc_.write(addr);
        f2i_valid_.write(1);
        pc_.write(addr + 4);
        return;
    }

    if (issue.pred_taken.read()) {
        u32 addr = issue.pred_target.read();
        u32 raw  = read_instruction(mem, addr);
        f2i_raw_.write(raw);
        f2i_pc_.write(addr);
        f2i_valid_.write(1);
        pc_.write(addr + 4);
        return;
    }

    if (issue.stall.read()) {
        return;
    }

    u32 addr = pc_.cur();
    u32 raw  = read_instruction(mem, addr);
    f2i_raw_.write(raw);
    f2i_pc_.write(addr);
    f2i_valid_.write(1);
    pc_.write(addr + 4);
}
