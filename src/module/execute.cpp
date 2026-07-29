#include "module/execute.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <cstdio>
#include <cstdlib>

static auto ALU_R(u32 func3, u32 func7, u32 rs1, u32 rs2) -> u32 {
    switch (func3) {
        case 0x0: // ADD / SUB
            return (func7 == 0) ? rs1 + rs2 : rs1 - rs2;
        case 0x1: // SLL
            return rs1 << (rs2 & 0x1F);
        case 0x2: // SLT
            return (i32)rs1 < (i32)rs2 ? 1 : 0;
        case 0x3: // SLTU
            return rs1 < rs2 ? 1 : 0;
        case 0x4: // XOR
            return rs1 ^ rs2;
        case 0x5: // SRL / SRA
            return (func7 == 0)
                ? rs1 >> (rs2 & 0x1F)
                : (u32)((i32)rs1 >> (rs2 & 0x1F));
        case 0x6: // OR
            return rs1 | rs2;
        case 0x7: // AND
            return rs1 & rs2;
        default:
            fprintf(stderr, "invalid func3 for ALU_R: 0x%x\n", func3);
            exit(1);
    }
}

static auto ALU_I(u32 func3, u32 func7, u32 rs1, u32 imm) -> u32 {
    switch (func3) {
        case 0x0: // ADDI
            return rs1 + imm;
        case 0x1: // SLLI
            return rs1 << (imm & 0x1F);
        case 0x2: // SLTI
            return (i32)rs1 < (i32)imm ? 1 : 0;
        case 0x3: // SLTIU
            return rs1 < imm ? 1 : 0;
        case 0x4: // XORI
            return rs1 ^ imm;
        case 0x5: // SRLI / SRAI
            return (func7 == 0)
                ? rs1 >> (imm & 0x1F)
                : (u32)((i32)rs1 >> (imm & 0x1F));
        case 0x6: // ORI
            return rs1 | imm;
        case 0x7: // ANDI
            return rs1 & imm;
        default:
            fprintf(stderr, "invalid func3 for ALU_I: 0x%x\n", func3);
            exit(1);
    }
}

static auto branch_cond(u32 func3, u32 rs1, u32 rs2) -> bool {
    switch (func3) {
        case 0x0: return rs1 == rs2;                  // BEQ
        case 0x1: return rs1 != rs2;                  // BNE
        case 0x4: return (i32)rs1 < (i32)rs2;         // BLT
        case 0x5: return (i32)rs1 >= (i32)rs2;        // BGE
        case 0x6: return rs1 < rs2;                   // BLTU
        case 0x7: return rs1 >= rs2;                  // BGEU
        default:
            fprintf(stderr, "invalid func3 for branch: 0x%x\n", func3);
            exit(1);
    }
}

static void flush_pipeline(CPUState &nxt, size_t branch_rob_tag) {
    //fprintf(stderr, "flush\n");
    size_t flush_start = (branch_rob_tag + 1) % ROB_SIZE;
    size_t flush_end   = nxt.rob.last;

    auto in_range = [&](size_t tag) -> bool {
        if (flush_start < flush_end)
            return tag >= flush_start && tag < flush_end;
        else
            return tag >= flush_start || tag < flush_end;
    };

    nxt.rob.last = flush_start;
    nxt.fetch.pred_taken = false;  // kill spurious pred_taken from flushed instructions

    for (int i = 0; i < RS_SIZE; i++) {
        if (nxt.rs.buf[i].valid && in_range(nxt.rs.buf[i].rob_tag)) {
            nxt.rs.buf[i].valid = false;
        }
    }

    for (int i = 0; i < LSQ_SIZE; i++) {
        if (nxt.lsq.buf[i].valid && in_range(nxt.lsq.buf[i].rob_tag)) {
            nxt.lsq.buf[i].valid = false;
        }
    }

    for (int i = 0; i < 32; i++) {
        if (nxt.rat.map[i] != NONE_ROB_TAG && in_range(nxt.rat.map[i])) {
            nxt.rat.map[i] = NONE_ROB_TAG;
        }
    }
}

void execute(const CPUState &cur, CPUState &nxt) {
    bool processed[RS_SIZE] = {false};
    bool mispredicted = false;

    while (true) {
        // Find the oldest ready RS entry by distance from rob.head.
        // Raw rob_tag comparison breaks when tags wrap around ROB_SIZE.
        int oldest_i = -1;
        size_t oldest_dist = SIZE_MAX;
        for (int i = 0; i < RS_SIZE; i++) {
            if (processed[i]) continue;
            if (!cur.rs.buf[i].valid) { processed[i] = true; continue; }
            if (!cur.rs.buf[i].ready1 || !cur.rs.buf[i].ready2) continue;
            size_t dist = (cur.rs.buf[i].rob_tag - cur.rob.head + ROB_SIZE) % ROB_SIZE;
            if (dist < oldest_dist) {
                oldest_i = i;
                oldest_dist = dist;
            }
        }
        if (oldest_i == -1) break;

        processed[oldest_i] = true;
        const RSEntry &rs = cur.rs.buf[oldest_i];
        u32 rs1 = rs.value1;
        u32 rs2 = rs.value2;
        u32 imm = rs.ins.imm;
        u32 result = 0;
        bool write_cdb = true;
        switch (rs.ins.opcode) {
            case 0x33:
                result = ALU_R(rs.ins.func3, rs.ins.func7, rs1, rs2);
                break;
            case 0x13:
                result = ALU_I(rs.ins.func3, rs.ins.func7, rs1, imm);
                break;
            case 0x37:
                result = imm;
                break;
            case 0x17:
                result = rs.pc + imm;
                break;
            case 0x03:
                result = rs1 + imm;
                nxt.lsq.buf[rs.lsq_tag].addr_ready = true;
                nxt.lsq.buf[rs.lsq_tag].addr = result;
                write_cdb = false;
                break;
            case 0x23:
                result = rs1 + imm;
                nxt.lsq.buf[rs.lsq_tag].addr_ready = true;
                nxt.lsq.buf[rs.lsq_tag].addr = result;
                nxt.lsq.buf[rs.lsq_tag].data_ready = true;
                switch (rs.ins.func3) {
                    case 0x0: nxt.lsq.buf[rs.lsq_tag].data = rs2 & 0xFF; break;
                    case 0x1: nxt.lsq.buf[rs.lsq_tag].data = rs2 & 0xFFFF; break;
                    case 0x2: nxt.lsq.buf[rs.lsq_tag].data = rs2; break;
                }
                nxt.rob.buf[rs.rob_tag].address = result;
                nxt.rob.buf[rs.rob_tag].result = rs2;
                nxt.rob.buf[rs.rob_tag].ready = true;
                write_cdb = false;
                break;
            case 0x63: {
                bool taken = branch_cond(rs.ins.func3, rs1, rs2);
                if (taken && !nxt.fetch.mispredict) {
                    nxt.fetch.mispredict = true;
                    nxt.fetch.correct_pc  = rs.pc + rs.ins.imm;
                    nxt.fetch.pred_taken = false;
                    flush_pipeline(nxt, rs.rob_tag);
                    write_cdb = false;
                    mispredicted = true;
                }
                nxt.rob.buf[rs.rob_tag].ready = true;
                write_cdb = false;
                break;
            }
            case 0x6F:
                result = rs.pc + 4;
                write_cdb = true;
                break;
            case 0x67: {
                u32 target = (rs1 + rs.ins.imm) & ~1u;
                result = rs.pc + 4;
                write_cdb = true;
                if (!nxt.fetch.mispredict) {
                    nxt.fetch.mispredict = true;
                    nxt.fetch.correct_pc  = target;
                    nxt.fetch.pred_taken = false;
                    flush_pipeline(nxt, rs.rob_tag);
                    mispredicted = true;
                }
                break;
            }
            case 0x0F:
                result = 0;
                write_cdb = false;
                break;
            default:
                fprintf(stderr, "unknown opcode in execute: 0x%x, raw=0x%08x pc=0x%08x\n",
                        rs.ins.opcode, rs.ins.raw, rs.pc);
                exit(1);
        }

        if (write_cdb) {
            nxt.cdb.push(rs.rob_tag, result);
        }
        nxt.rs.buf[oldest_i].valid = false;

        if (mispredicted) break;
    }
}
