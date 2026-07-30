#include "module/execute.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <cstdio>
#include <cstdlib>

size_t branch_count = 0;
size_t mispredict_count = 0;

static auto ALU_R(u32 func3, u32 func7, u32 rs1, u32 rs2) -> u32 {
    switch (func3) {
        case 0x0: return (func7 == 0) ? rs1 + rs2 : rs1 - rs2; // ADD / SUB
        case 0x1: return rs1 << (rs2 & 0x1F);                   // SLL
        case 0x2: return (i32)rs1 < (i32)rs2 ? 1 : 0;          // SLT
        case 0x3: return rs1 < rs2 ? 1 : 0;                    // SLTU
        case 0x4: return rs1 ^ rs2;                            // XOR
        case 0x5: return (func7 == 0) ? rs1 >> (rs2 & 0x1F) 
                                      : (u32)((i32)rs1 >> (rs2 & 0x1F)); // SRL / SRA
        case 0x6: return rs1 | rs2;                            // OR
        case 0x7: return rs1 & rs2;                            // AND
        default:
            fprintf(stderr, "invalid func3 for ALU_R: 0x%x\n", func3);
            exit(1);
    }
}

static auto ALU_I(u32 func3, u32 func7, u32 rs1, u32 imm) -> u32 {
    switch (func3) {
        case 0x0: return rs1 + imm;                            // ADDI
        case 0x1: return rs1 << (imm & 0x1F);                  // SLLI
        case 0x2: return (i32)rs1 < (i32)imm ? 1 : 0;          // SLTI
        case 0x3: return rs1 < imm ? 1 : 0;                    // SLTIU
        case 0x4: return rs1 ^ imm;                            // XORI
        case 0x5: return (func7 == 0) ? rs1 >> (imm & 0x1F) 
                                      : (u32)((i32)rs1 >> (imm & 0x1F)); // SRLI / SRAI
        case 0x6: return rs1 | imm;                            // ORI
        case 0x7: return rs1 & imm;                            // ANDI
        default:
            fprintf(stderr, "invalid func3 for ALU_I: 0x%x\n", func3);
            exit(1);
    }
}

static auto branch_cond(u32 func3, u32 rs1, u32 rs2) -> bool {
    switch (func3) {
        case 0x0: return rs1 == rs2;           // BEQ
        case 0x1: return rs1 != rs2;           // BNE
        case 0x4: return (i32)rs1 < (i32)rs2;  // BLT
        case 0x5: return (i32)rs1 >= (i32)rs2; // BGE
        case 0x6: return rs1 < rs2;            // BLTU
        case 0x7: return rs1 >= rs2;           // BGEU
        default:
            fprintf(stderr, "invalid func3 for branch: 0x%x\n", func3);
            exit(1);
    }
}

static void flush_pipeline(CPUState &nxt, const CPUState &cur, size_t branch_rob_tag) {
    size_t flush_start = (branch_rob_tag + 1) % ROB_SIZE;
    size_t flush_end   = nxt.rob.last;

    auto in_range = [&](size_t tag) -> bool {
        if (flush_start < flush_end) {
            return tag >= flush_start && tag < flush_end;
        } else {
            return tag >= flush_start || tag < flush_end;
        }
    };

    size_t curr_tag = (flush_end + ROB_SIZE - 1) % ROB_SIZE;
    while (curr_tag != branch_rob_tag) {
        const ROBEntry &entry = nxt.rob.buf[curr_tag];
        if (entry.arch_dest != 0 && entry.new_pnum != 0) {
            nxt.rat.map[entry.arch_dest] = entry.old_pnum;
            nxt.free_list.push(entry.new_pnum);
            nxt.ready_table.ready[entry.new_pnum] = false;
        }
        if (curr_tag == flush_start) break;
        curr_tag = (curr_tag + ROB_SIZE - 1) % ROB_SIZE;
    }

    nxt.rob.last = flush_start;
    nxt.fetch.pred_taken = false;

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
}

static bool fetchOperand(const CPUState &cur, PhysRegNum prs, u32 &out_val) {
    if (prs == 0) {
        out_val = 0;
        return true;
    }

    for (int c = 0; c < CDB_SIZE; c++) {
        if (cur.cdb.buf[c].valid && cur.cdb.buf[c].prd == prs) {
            out_val = cur.cdb.buf[c].result;
            return true;
        }
    }

    if (cur.ready_table.ready[prs]) {
        out_val = cur.prf.values[prs];
        return true;
    }

    out_val = 0;
    return false;
}

void execute(const CPUState &cur, CPUState &nxt) {
    bool processed[RS_SIZE] = {false};
    bool mispredicted = false;

    int alu_exec_count = 0;
    const int MAX_ALU_EXEC = 2;

    while (alu_exec_count < MAX_ALU_EXEC) {
        int oldest_i = -1;
        size_t oldest_dist = SIZE_MAX;
        size_t jalr_tag = NONE_ROB_TAG;
        size_t jalr_dist = SIZE_MAX;

        for (int i = 0; i < RS_SIZE; i++) {
            if (processed[i]) continue;

            if (!cur.rs.buf[i].valid) {
                processed[i] = true; 
                continue; 
            }
            //if (cur.rs.buf[i].pc == 0x1008) fprintf(stderr, "in RS %d %d\n", cur.ready_table.ready[cur.rs.buf[i].prs1], cur.ready_table.ready[cur.rs.buf[i].prs2]);

            size_t dist = (cur.rs.buf[i].rob_tag - cur.rob.head + ROB_SIZE) % ROB_SIZE;
            u32 tmp;
            if (!fetchOperand(cur, cur.rs.buf[i].prs1, tmp) || !fetchOperand(cur, cur.rs.buf[i].prs2, tmp)) {
                if (cur.rs.buf[i].ins.opcode == 0x67 && dist < jalr_dist) {
                    jalr_tag = cur.rs.buf[i].rob_tag;
                    jalr_dist = dist;
                }
                continue;
            }

            if (jalr_tag != NONE_ROB_TAG) {
                size_t start = (jalr_tag + 1) % ROB_SIZE;
                size_t end   = nxt.rob.last;
                bool newer;
                if (start < end)
                    newer = cur.rs.buf[i].rob_tag >= start && cur.rs.buf[i].rob_tag < end;
                else
                    newer = cur.rs.buf[i].rob_tag >= start || cur.rs.buf[i].rob_tag < end;
                
                if (newer) continue;
            }

            if (dist < oldest_dist) {
                oldest_i = i;
                oldest_dist = dist;
            }
        }

        if (oldest_i == -1) break;

        processed[oldest_i] = true;
        const RSEntry &rs = cur.rs.buf[oldest_i];

        u32 rs1 = 0, rs2 = 0;
        fetchOperand(cur, rs.prs1, rs1);
        fetchOperand(cur, rs.prs2, rs2);

        u32 imm = rs.ins.imm;
        u32 result = 0;
        bool write_cdb = true;

        //if (rs.pc == 0x1008) fprintf(stderr, "exe\n");


        switch (rs.ins.opcode) {
            case 0x33: // R-type ALU
                result = ALU_R(rs.ins.func3, rs.ins.func7, rs1, rs2);
                break;

            case 0x13: // I-type ALU
                result = ALU_I(rs.ins.func3, rs.ins.func7, rs1, imm);
                break;

            case 0x37: // LUI
                result = imm;
                break;

            case 0x17: // AUIPC
                result = rs.pc + imm;
                break;

            case 0x03: // Load
                result = rs1 + imm;
                nxt.lsq.buf[rs.lsq_tag].addr_ready = true;
                nxt.lsq.buf[rs.lsq_tag].addr = result;
                write_cdb = false;
                break;

            case 0x23: // Store
                result = rs1 + imm;
                nxt.lsq.buf[rs.lsq_tag].addr_ready = true;
                nxt.lsq.buf[rs.lsq_tag].addr = result;
                nxt.lsq.buf[rs.lsq_tag].data_ready = true;
                switch (rs.ins.func3) {
                    case 0x0: nxt.lsq.buf[rs.lsq_tag].data = rs2 & 0xFF; break;
                    case 0x1: nxt.lsq.buf[rs.lsq_tag].data = rs2 & 0xFFFF; break;
                    case 0x2: nxt.lsq.buf[rs.lsq_tag].data = rs2; break;
                }
                nxt.rob.buf[rs.rob_tag].ready = true;
                write_cdb = false;
                break;

            case 0x63: { // Branch
                branch_count++;
                bool taken = branch_cond(rs.ins.func3, rs1, rs2);
                if (taken && !nxt.fetch.mispredict) {
                    mispredict_count++;
                    nxt.fetch.mispredict = true;
                    nxt.fetch.correct_pc  = rs.pc + rs.ins.imm;
                    nxt.fetch.pred_taken = false;
                    flush_pipeline(nxt, cur, rs.rob_tag);
                    write_cdb = false;
                    mispredicted = true;
                }
                nxt.rob.buf[rs.rob_tag].ready = true;
                write_cdb = false;
                break;
            }

            case 0x6F: // JAL
                result = rs.pc + 4;
                write_cdb = true;
                break;

            case 0x67: { // JALR
                u32 target = (rs1 + rs.ins.imm) & ~1u;
                result = rs.pc + 4;
                write_cdb = true;
                if (!nxt.fetch.mispredict) {
                    nxt.fetch.mispredict = true;
                    nxt.fetch.correct_pc  = target;
                    nxt.fetch.pred_taken = false;
                    flush_pipeline(nxt, cur, rs.rob_tag);
                    mispredicted = true;
                }
                break;
            }

            case 0x0F: // FENCE
                result = 0;
                write_cdb = false;
                break;

            default:
                fprintf(stderr, "unknown opcode in execute: 0x%x, raw=0x%08x pc=0x%08x\n",
                        rs.ins.opcode, rs.ins.raw, rs.pc);
                exit(1);
        }

        if (write_cdb) {
            nxt.cdb.push(rs.prd, result, rs.rob_tag);
        }

        nxt.rs.buf[oldest_i].valid = false;
        alu_exec_count++;

        if (mispredicted) break;
    }
}