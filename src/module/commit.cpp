#include "module/commit.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"

void commit(const CPUState &cur, CPUState &nxt) {
    while (nxt.rob.head != nxt.rob.last && cur.rob.buf[nxt.rob.head].ready) {
        size_t commit_tag = nxt.rob.head;
        const ROBEntry &rob = cur.rob.buf[nxt.rob.head++];
        nxt.rob.head %= ROB_SIZE;
        //fprintf(stderr, "commit rob_tag=%zu opcode=0x%x rd=%u result=0x%x head=%zu last=%zu\n",
        //        commit_tag, rob.ins.opcode, rob.ins.rd, rob.result, nxt.rob.head, nxt.rob.last);
        if (
            rob.ins.opcode == 0x33 ||  //ALU_R
            rob.ins.opcode == 0x13 ||  //ALU_I
            rob.ins.opcode == 0x3 ||   //load
            rob.ins.opcode == 0x6F ||  //jal
            rob.ins.opcode == 0x67 ||  //jalr
            rob.ins.opcode == 0x17 ||  //auipc
            rob.ins.opcode == 0x37     //lui
        ) {
            nxt.reg.reg[rob.ins.rd] = rob.result;
            // if (rob.ins.rd == 10) {
            //     fprintf(stderr, "write x10: commit_tag=%zu opcode=0x%x result=%d\n",
            //             commit_tag, rob.ins.opcode, rob.result);
            // }
            if (nxt.rat.map[rob.ins.rd] == commit_tag) {
                nxt.rat.map[rob.ins.rd] = NONE_ROB_TAG;
            }
            if (rob.ins.opcode == 0x3) {
                nxt.lsq.buf[rob.lsq_tag].valid = false;
            }
        }
        if (rob.ins.opcode == 0x23) {
            // if (rob.address == 0x106c) {
            //     fprintf(stderr, "STORE to 0x106c: commit_tag=%zu data=%d\n",
            //             commit_tag, rob.result);
            // }
            nxt.memory.data[rob.address] = rob.result & 0xFF;
            nxt.memory.data[rob.address + 1] = (rob.result >> 8) & 0xFF;
            nxt.memory.data[rob.address + 2] = (rob.result >> 16) & 0xFF;
            nxt.memory.data[rob.address + 3] = (rob.result >> 24) & 0xFF;
            nxt.lsq.buf[rob.lsq_tag].valid = false;
        }
    }
    while (nxt.lsq.head != nxt.lsq.last && !nxt.lsq.buf[nxt.lsq.head].valid) {
        nxt.lsq.head = (nxt.lsq.head + 1) % LSQ_SIZE;
    }
}