#include "storage/fetch_module.hpp"

static u32 sign_extend(u32 v, u8 b) {
    if (v & (1u << (b - 1))) return v | (~((1u << b) - 1));
    return v & ((1u << b) - 1);
}

static const bool RAS_ENABLED = true;

void FetchModule::eval(
    const HaltRequestWritePorts &halt_req,
    const ExecToFetchWritePorts &exec,
    const IssueToFetchWritePorts &issue,
    const BHTReadPorts &bht,
    const RASReadPorts &ras,
    const FetchRASWritePorts &ras_fetch,
    const MemoryModule &mem
) {
    pc_.hold();
    f2i_raw_.hold();
    f2i_pc_.hold();
    f2i_valid_.hold();
    f2i_pred_.hold();
    f2i_pred_target_.hold();
    halt_.hold();

    if (halt_req.req.read()) {
        halt_.write(1);
    }
    if (halt_.cur()) {
        return;
    }

    u32 addr;
    if (exec.mispredict.read()) {
        addr = exec.correct_pc.read();
    } else if (issue.stall.read()) {
        return;
    } else {
        addr = pc_.cur();
    }

    u32 raw = read_instruction(mem, addr);
    u32 opcode = raw & 0x7f;
    u32 rd = (raw >> 7) & 0x1f;
    u32 rs1 = (raw >> 15) & 0x1f;

    u32 next_pc = addr + 4;
    u32 pred_taken = 0;
    u32 pred_target = 0;

    if (opcode == 0x63) {
        u32 imm = sign_extend(((raw >> 19) & 0x1000) | ((raw << 4) & 0x800) |
                              ((raw >> 20) & 0x7E0) | ((raw >> 7) & 0x1E), 13);
        if (bht.counters[(addr >> 2) % BHT_SIZE].read() >= 2) {
            pred_taken = 1;
            pred_target = addr + imm;
        }
    } else if (opcode == 0x6F) {
        u32 imm = sign_extend(((raw >> 11) & 0x100000) | (raw & 0xFF000) |
                              ((raw >> 9) & 0x800) | ((raw >> 20) & 0x7FE), 21);
        pred_taken = 1;
        pred_target = addr + imm;
    } else if (opcode == 0x67) {
        if (rd == 0 && rs1 == 1) {
            // ret: predict taken to the RAS top, pop
            if (!ras.empty.read() && RAS_ENABLED) {
                pred_taken = 1;
                pred_target = ras.top.read();
                ras_fetch.pop_valid.write(1);
            }
        } else if (rd == 0) {
            if (!ras.empty.read()) {
                ras_fetch.pop_valid.write(1);
            }
        }
    }

    if ((opcode == 0x6F || opcode == 0x67) && rd == 1) {
        ras_fetch.push_valid.write(1);
        ras_fetch.push_val.write(addr + 4);
    }

    f2i_raw_.write(raw);
    f2i_pc_.write(addr);
    f2i_valid_.write(1);
    f2i_pred_.write(pred_taken);
    f2i_pred_target_.write(pred_target);
    f2i_ras_snap_.write((opcode == 0x63 || opcode == 0x67) ? ras.head.read() : 0);
    pc_.write(pred_taken ? pred_target : addr + 4);
}
