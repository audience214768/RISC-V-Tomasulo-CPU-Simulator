#include "module/issue.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <cstdio>
#include <cstdlib>

static auto sign_extend(u32 val, u8 bits) -> u32 {
    if (val & (1u << (bits - 1))) {
        return val | (~((1u << bits) - 1));
    }
    return val & ((1u << bits) - 1);
}

auto decode(u32 raw) ->Instruction {
    u32 opcode = raw & 0x7f;
    u32 imm = 0;
    switch (opcode) {
        case 0x03: // LOAD: lb/lh/lw/lbu/lhu
        case 0x13: // OP-IMM: addi/slti/sltiu/xori/ori/andi/slli/srli/srai
        case 0x67: // JALR
        case 0x73: // SYSTEM: ecall/ebreak
            imm = sign_extend((raw >> 20) & 0xFFF, 12);
            break;
        case 0x23: // STORE: sb/sh/sw
            imm = sign_extend(((raw >> 20) & 0xFE0) | ((raw >> 7) & 0x1F), 12);
            break;
        case 0x63: // BRANCH: beq/bne/blt/bge/bltu/bgeu
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
        default: // 0x33 (R-type), 0x0F (FENCE) etc.
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
    // if (cur.fetch.instruction_pc == 0x11ac) {
    //     fprintf(stderr, "find the instruction raw = 0x%0x\n", ins.raw);
    // }
    //static int cnt888 = 0;
    // if (cur.fetch.instruction_pc == 0x109c) {
    //     fprintf(stderr, "ISSUE 888-store #%d: rob=%zu\n", ++cnt888, cur.rob.last);
    // }

    // if (cur.fetch.instruction_pc == 0x10a0) {
    //     fprintf(stderr, "the a5 is %d\n", cur.reg.reg[15]);
    // }

    if (cur.fetch.raw_instruction == TERMINATE_INST) {
        nxt.fetch.halt = true;
        return;
    }
    if (nxt.rob.full()) {
        fprintf(stderr, "rob is full\n");
        exit(1);
    }
    size_t lsq_tag = 0;
    //fprintf(stderr, "check\n");
    if (ins.opcode == 0x3 || ins.opcode == 0x23) {
        LSQEntry lsq = LSQEntry {
            .valid = true,
            .is_load = (ins.opcode == 0x3),
            .rob_tag = cur.rob.last,
            .addr_ready = false,
            .data_ready = false,
        };
        switch (ins.func3) {
            case 0x0: // lb / sb
                lsq.width = 1;
                lsq.is_unsigned = false;
                break;
            case 0x1: // lh / sh
                lsq.width = 2;
                lsq.is_unsigned = false;
                break;
            case 0x2: // lw / sw
                //fprintf(stderr, "load: lw\n");
                lsq.width = 4;
                lsq.is_unsigned = false;
                break;
            case 0x4: // lbu
                lsq.width = 1;
                lsq.is_unsigned = true;
                break;
            case 0x5: // lhu
                lsq.width = 2;
                lsq.is_unsigned = true;
                break;
        }
        lsq_tag = nxt.lsq.push(lsq);
    }
    
    auto rs_entry = RSEntry {
        .valid = true,
        .ins = ins,
        .rob_tag = cur.rob.last,
        .lsq_tag = lsq_tag,
        .pc = cur.fetch.instruction_pc,
    };
    if (
        ins.opcode == 0x6F ||
        ins.opcode == 0x17 ||
        ins.opcode == 0x37 ||
        ins.opcode == 0x73
    ) {
        rs_entry.ready1 = true;
        rs_entry.value1 = 0;
    } else if (cur.rat.map[ins.rs1] == NONE_ROB_TAG) {
        rs_entry.ready1 = true;
        rs_entry.value1 = cur.reg.reg[ins.rs1];
    } else if (cur.rob.buf[cur.rat.map[ins.rs1]].ready) {
        rs_entry.ready1 = true;
        rs_entry.value1 = cur.rob.buf[cur.rat.map[ins.rs1]].result;
    } else {
        rs_entry.ready1 = false;
        rs_entry.query1 = cur.rat.map[ins.rs1];
    }
    if (
        ins.opcode == 0x13 || 
        ins.opcode == 0x3 || 
        ins.opcode == 0x67 && ins.func3 == 0x0 || 
        ins.opcode == 0x6F ||
        ins.opcode == 0x17 ||
        ins.opcode == 0x37 ||
        ins.opcode == 0x73
    ) {
        rs_entry.ready2 = true;
        rs_entry.value2 = 0;
    } else if (cur.rat.map[ins.rs2] == NONE_ROB_TAG) {
        rs_entry.ready2 = true;
        rs_entry.value2 = cur.reg.reg[ins.rs2];
    } else if (cur.rob.buf[cur.rat.map[ins.rs2]].ready) {
        rs_entry.ready2 = true;
        rs_entry.value2 = cur.rob.buf[cur.rat.map[ins.rs2]].result;
    } else {
        rs_entry.ready2 = false;
        rs_entry.query2 = cur.rat.map[ins.rs2];
    }
    if (!rs_entry.ready1) {
        for (int j = 0; j < CDB_SIZE; j++) {
            if (cur.cdb.buf[j].valid && cur.cdb.buf[j].rob_tag == rs_entry.query1) {
                rs_entry.ready1 = true;
                rs_entry.value1 = cur.cdb.buf[j].result;
                break;
            }
        }
    }
    if (!rs_entry.ready2) {
        for (int j = 0; j < CDB_SIZE; j++) {
            if (cur.cdb.buf[j].valid && cur.cdb.buf[j].rob_tag == rs_entry.query2) {
                rs_entry.ready2 = true;
                rs_entry.value2 = cur.cdb.buf[j].result;
                break;
            }
        }
    }
    nxt.rs.push(rs_entry);

    for (int i = 0; i < RS_SIZE; i++) {
        if (!cur.rs.buf[i].ready1) {
            for (int j = 0; j < CDB_SIZE; j++) {
                if (cur.cdb.buf[j].valid && cur.rs.buf[i].query1 == cur.cdb.buf[j].rob_tag) {
                    nxt.rs.buf[i].ready1 = true;
                    nxt.rs.buf[i].value1 = cur.cdb.buf[j].result;
                    break;
                }
            }
        }
        if (!cur.rs.buf[i].ready2) {
            for (int j = 0; j < CDB_SIZE; j++) {
                if (cur.cdb.buf[j].valid && cur.rs.buf[i].query2 == cur.cdb.buf[j].rob_tag) {
                    nxt.rs.buf[i].ready2 = true;
                    nxt.rs.buf[i].value2 = cur.cdb.buf[j].result;
                    break;
                }
            }
        }
    }

    if (ins.opcode == 0x6F) {
        nxt.fetch.pred_taken  = true;
        nxt.fetch.pred_target = cur.fetch.instruction_pc + ins.imm;
    }

    if (
        ins.opcode != 0x23 &&
        ins.opcode != 0x63 &&
        ins.opcode != 0x73 &&
        ins.rd != 0 //the x0 is always reset so can't be renamed!!!
    ) {
        nxt.rat.map[ins.rd] = cur.rob.last;
    }

    nxt.rob.buf[cur.rob.last] = ROBEntry {
        .ready = false,
        .ins = ins,
        .result = 0,
        .lsq_tag = lsq_tag
    };
    nxt.rob.last = (cur.rob.last + 1) % ROB_SIZE;
}

