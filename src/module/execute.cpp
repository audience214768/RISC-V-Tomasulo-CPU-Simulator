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
    for (int i = 0; i < RS_SIZE; i++) {
        const RSEntry &rs = cur.rs.buf[i];
        u32 rs1; u32 rs2; u32 imm = rs.ins.imm;
        if (!rs.valid) continue;
        //fprintf(stderr, "rsstate raw = 0x%0x rs1 = %d rs2 = %d\n", rs.ins.raw, rs.value1, rs.value2);
        if (!rs.ready1) {
            bool find = false;
            for (int j = 0; j < CDB_SIZE; j++) {
                if (cur.cdb.buf[j].valid && cur.cdb.buf[j].rob_tag == rs.query1) {
                    find = true;
                    rs1 = cur.cdb.buf[j].result;
                    break;
                }
            }
            if (!find) {
                continue;
            }
        } else {
            rs1 = rs.value1;
        }
        if (!rs.ready2) {
            bool find = false;
            for (int j = 0; j < CDB_SIZE; j++) {
                if (cur.cdb.buf[j].valid && cur.cdb.buf[j].rob_tag == rs.query2) {
                    find = true;
                    rs2 = cur.cdb.buf[j].result;
                    break;
                }
            }
            if (!find) {
                continue;
            }
        } else {
            rs2 = rs.value2;
        }
        u32 result = 0;
        bool write_cdb = true;
        bool free_rs   = true;
        switch (rs.ins.opcode) {
            // ─── R-type ALU: OP (0x33) ───
            case 0x33:
                result = ALU_R(rs.ins.func3, rs.ins.func7, rs1, rs2);
                //if (rs.pc == 0x1008) fprintf(stderr, "XOR exec: rs1=%d rs2=%d -> %d\n", rs1, rs2, result);
                break;
            // ─── I-type ALU: OP_IMM (0x13) ───
            case 0x13:
                result = ALU_I(rs.ins.func3, rs.ins.func7, rs1, imm);
                break;
            // ─── LUI (0x37) ───
            case 0x37:
                result = imm;
                break;
            // ─── AUIPC (0x17) ───
            case 0x17:
                result = rs.pc + imm;
                break;
            // ─── LOAD (0x03) ───
            case 0x03:
                result = rs1 + imm;
                nxt.lsq.buf[rs.lsq_tag].addr_ready = true;
                nxt.lsq.buf[rs.lsq_tag].addr = result;
                write_cdb = false;
                break;
            // ─── STORE (0x23) ───
            case 0x23: //目前没有实现分布计算
                result = rs1 + imm;
                nxt.lsq.buf[rs.lsq_tag].addr_ready = true;
                nxt.lsq.buf[rs.lsq_tag].addr = result;
                nxt.lsq.buf[rs.lsq_tag].data_ready = true;
                switch (rs.ins.func3) {
                    case 0x0:
                        nxt.lsq.buf[rs.lsq_tag].data = rs2 & 0xFF;
                        break;
                    case 0x1:
                        nxt.lsq.buf[rs.lsq_tag].data = rs2 & 0xFFFF;
                        break;
                    case 0x2:
                        nxt.lsq.buf[rs.lsq_tag].data = rs2;
                        break;
                }
                // if (rs.ins.raw == 0x00e7a023) {
                //     fprintf(stderr, "instruction 00e7a023 result = %d data = %d\n", result, rs2);
                // }
                nxt.rob.buf[rs.rob_tag].address = result;
                nxt.rob.buf[rs.rob_tag].result = rs2;
                nxt.rob.buf[rs.rob_tag].ready = true;
                write_cdb = false;
                break;
            // ─── BRANCH (0x63): conditional ───
            case 0x63: {
                bool taken = branch_cond(rs.ins.func3, rs1, rs2);
                if (taken && !nxt.fetch.mispredict) {
                    nxt.fetch.mispredict = true;
                    nxt.fetch.correct_pc  = rs.pc + rs.ins.imm;
                    nxt.fetch.pred_taken = false;
                    flush_pipeline(nxt, rs.rob_tag);
                }
                nxt.rob.buf[rs.rob_tag].ready = true;
                write_cdb = false;
                break;
            }
            // ─── JAL (0x6F) ───
            case 0x6F:
                result = rs.pc + 4;
                write_cdb = true;
                break;
            // ─── JALR (0x67) ───
            case 0x67: {
                u32 target = (rs1 + rs.ins.imm) & ~1u;
                result = rs.pc + 4;
                write_cdb = true;
                if (!nxt.fetch.mispredict) {
                    nxt.fetch.mispredict = true;
                    nxt.fetch.correct_pc  = target;
                    nxt.fetch.pred_taken = false;
                    flush_pipeline(nxt, rs.rob_tag);
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

        // if (rs.pc >= 0x10f0 && rs.pc <= 0x1198) {
        //     fprintf(stderr, "div exec: pc=0x%x op=0x%x rd=%u r1=%d r2=%d imm=%d res=%d\n",
        //             rs.pc, rs.ins.opcode, rs.ins.rd, rs1, rs2, imm, result);
        // }

        if (write_cdb) {
            //fprintf(stderr, "write cdb 0x%0x\n", rs.ins.raw);
            nxt.cdb.push(rs.rob_tag, result);
        }

        nxt.rs.buf[i].valid = false;
    }
}
