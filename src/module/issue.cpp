#include "module/issue.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <cstdio>

static auto sign_extend(u32 val, u8 bits) -> u32 {
    if (val & (1u << (bits - 1))) {
        return val | (~((1u << bits) - 1));
    }
    return val & ((1u << bits) - 1);
}

auto decode(u32 raw) -> Instruction {
    u32 opcode = raw & 0x7f;
    u32 imm = 0;
    switch (opcode) {
        case 0x03: // LOAD
        case 0x13: // OP-IMM
        case 0x67: // JALR
        case 0x73: // SYSTEM
            imm = sign_extend((raw >> 20) & 0xFFF, 12);
            break;
        case 0x23: // STORE
            imm = sign_extend(((raw >> 20) & 0xFE0) | ((raw >> 7) & 0x1F), 12);
            break;
        case 0x63: // BRANCH
            imm = sign_extend(
                ((raw >> 19) & 0x1000) |
                ((raw << 4)  & 0x0800) |
                ((raw >> 20) & 0x07E0) |
                ((raw >> 7)  & 0x001E),
                13);
            break;
        case 0x37: // LUI
        case 0x17: // AUIPC
            imm = raw & 0xFFFFF000;
            break;
        case 0x6F: // JAL
            imm = sign_extend(
                ((raw >> 11) & 0x100000) |
                ((raw >>  0) & 0x0FF000) |
                ((raw >>  9) & 0x000800) |
                ((raw >> 20) & 0x0007FE),
                21);
            break;
        default:
            imm = 0;
            break;
    }
    return Instruction {
        .raw = raw,
        .opcode = opcode,
        .func3 = (raw >> 12) & 0x7,
        .func7 = (raw >> 25) & 0x7F,
        .rd = (raw >> 7) & 0x1F,
        .rs1 = (raw >> 15) & 0x1F,
        .rs2 = (raw >> 20) & 0x1F,
        .imm = imm,
    };
}

void issue(const CPUState &cur, CPUState &nxt) {
    if (cur.fetch.halt) return;

    if (cur.fetch.mispredict || nxt.fetch.mispredict) return;
    if (cur.fetch.pred_taken || nxt.fetch.pred_taken) return;

    if (cur.fetch.raw_instruction == 0) return;

    auto ins = decode(cur.fetch.raw_instruction);

    //fprintf(stderr, "%0x\n", cur.fetch.raw_instruction);

    if (cur.fetch.raw_instruction == TERMINATE_INST) {
        nxt.fetch.halt = true;
        //fprintf(stderr, "halt");
        return;
    }

    bool writes_rf = (ins.opcode != 0x23 && // Store
                      ins.opcode != 0x63 && // Branch
                      ins.opcode != 0x73 && // System
                      ins.rd != 0);         // x0 always 0

    bool is_mem_op = (ins.opcode == 0x3 || ins.opcode == 0x23); // Load / Store

    bool rob_full = cur.rob.full();

    bool rs_full = cur.rs.full();
    
    bool freelist_full = writes_rf && cur.free_list.empty();

    bool lsq_full = is_mem_op && cur.lsq.full();

    if (rob_full || rs_full || freelist_full || lsq_full) {
        nxt.fetch = cur.fetch;
        
        nxt.rat = cur.rat;
        nxt.free_list = cur.free_list;

        return;
    }

    size_t lsq_tag = 0;
    if (is_mem_op) {
        PhysRegNum prs2_or_prd = (ins.opcode == 0x23) ? cur.rat.map[ins.rs2] : 0;

        LSQEntry lsq = LSQEntry {
            .valid = true,
            .is_load = (ins.opcode == 0x3),
            .rob_tag = cur.rob.last,
            .addr_ready = false,
            .data_ready = false,
            .prs2_or_prd = prs2_or_prd,
            .data = 0,
            .mem_wait = 0,
        };
        switch (ins.func3) {
            case 0x0: lsq.width = 1; lsq.is_unsigned = false; break;
            case 0x1: lsq.width = 2; lsq.is_unsigned = false; break;
            case 0x2: lsq.width = 4; lsq.is_unsigned = false; break;
            case 0x4: lsq.width = 1; lsq.is_unsigned = true;  break;
            case 0x5: lsq.width = 2; lsq.is_unsigned = true;  break;
        }
        lsq_tag = nxt.lsq.push(lsq);
    }

    PhysRegNum prs1 = cur.rat.map[ins.rs1];
    PhysRegNum prs2 = cur.rat.map[ins.rs2];
    PhysRegNum new_pnum = 0;
    PhysRegNum old_pnum = 0;

    if (writes_rf) {
        new_pnum = nxt.free_list.pop();
        //if (new_pnum == 44) fprintf(stderr, "the pc is %0x\n", cur.fetch.instruction_pc);
        old_pnum = cur.rat.map[ins.rd];

        nxt.rat.map[ins.rd] = new_pnum;
        nxt.ready_table.ready[new_pnum] = false;

        if (ins.opcode == 0x3) {
            nxt.lsq.buf[lsq_tag].prs2_or_prd = new_pnum;
        }
    }

    auto rs_entry = RSEntry {
        .valid = true,
        .ins = ins,
        .rob_tag = cur.rob.last,
        .lsq_tag = lsq_tag,
        .prs1 = prs1,
        .prs2 = prs2,
        .prd = new_pnum,
        .pc = cur.fetch.instruction_pc,
    };
    nxt.rs.push(rs_entry);

    if (ins.opcode == 0x6F) {
        nxt.fetch.pred_taken  = true;
        nxt.fetch.pred_target = cur.fetch.instruction_pc + ins.imm;
    }

    nxt.rob.buf[cur.rob.last] = ROBEntry {
        .ready = false,
        .ins = ins,
        .arch_dest = ins.rd,
        .new_pnum = new_pnum,
        .old_pnum = old_pnum,
        .lsq_tag = lsq_tag,
    };

    nxt.rob.last = (cur.rob.last + 1) % ROB_SIZE;
}