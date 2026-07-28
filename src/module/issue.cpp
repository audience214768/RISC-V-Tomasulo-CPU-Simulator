#include "module/issue.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <cstdio>
#include <cstdlib>

auto decode(u32 raw) ->Instruction {
    return Instruction {
        .opcode = raw & 0x7f,
        .func3 = (raw >> 12) & 0x7,
        .func7 = (raw >> 25) & 0x7,
        .rd = (raw >> 7) & 0x1F,
        .rs1 = (raw >> 15) & 0x1F,
        .rs2 = (raw >> 20) & 0x1F,
    };
}

void issue(const CPUState &cur, CPUState &nxt) {
    auto ins = decode(cur.fetch.raw_instruction);
    if (nxt.rob.full()) {
        fprintf(stderr, "rob is full\n");
        exit(1);
    }
    size_t lsq_tag;
    if (ins.opcode == 0x3 || ins.opcode == 0x23) {
        LSQEntry lsq = LSQEntry {
            .valid = true,
            .is_load = (ins.opcode == 0x3),
            .rob_tag = cur.rob.last,
            .addr_ready = false,
            .data_ready = false,
        };
        switch (ins.func3) {
            case 0x0:
                lsq.width = 1;
                lsq.is_unsigned = false;
                break;
            case 0x4:
                lsq.width = 1;
                lsq.is_unsigned = true;
                break;
            case 0x5:
                lsq.width = 2;  
                lsq.is_unsigned = false;          
            case 0x1:
                lsq.width = 2;
                lsq.is_unsigned = true;
                break;
            case 0x02:
                lsq.width = 4;
                lsq.is_unsigned = false;
                break;
        }
        lsq_tag = nxt.lsq.push(lsq);
    }
    
    auto rs_entry = RSEntry {
        .valid = true,
        .ins = ins,
        .rob_tag = cur.rob.last,
        .lsq_tag = lsq_tag,
        .pc = cur.fetch.pc,
    };
    if (
        ins.opcode == 0x6F ||
        ins.opcode == 0x17 ||
        ins.opcode == 0x37 ||
        ins.opcode == 0x73
    ) {

    } else if (cur.rat.map[ins.rs1] == NONE_ROB_TAG) {
        rs_entry.ready1 = true;
        rs_entry.value1 = cur.reg.reg[ins.rs1];
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
    } else {
        rs_entry.ready2 = false;
        rs_entry.query2 = cur.rat.map[ins.rs2];
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

    if (
        ins.opcode != 0x23 &&
        ins.opcode != 0x63 &&
        ins.opcode != 0x73
    ) {
        nxt.rat.map[ins.rd] = cur.rob.last;
    }

    nxt.rob.buf[(cur.rob.last + 1) % ROB_SIZE] = ROBEntry {
        .ready = false,
        .ins = ins,
        .result = 0,
        .lsq_tag = lsq_tag
    };
    nxt.rob.last = (cur.rob.last + 1) % ROB_SIZE;
}

