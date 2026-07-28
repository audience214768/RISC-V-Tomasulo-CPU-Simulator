#include "module/commit.hpp"
#include "utils/types.hpp"

void commit(const CPUState &cur, CPUState &nxt) {
    while (nxt.rob.head != nxt.rob.last && cur.rob.buf[nxt.rob.head].ready) {
        const ROBEntry &rob = cur.rob.buf[nxt.rob.head++];
        if (
            rob.ins.opcode == 0x33 ||  //ALU_R
            rob.ins.opcode == 0x13 ||  //ALU_I
            rob.ins.opcode == 0x3 ||   //load  
            rob.ins.opcode == 0x6F ||  //jal
            rob.ins.opcode == 0x67 ||  //jalr
            rob.ins.opcode == 0x17 ||  //auipc
            rob.ins.opcode == 0x37     //lui
        ) {
            if (rob.ins.opcode == 0x3) {
                nxt.lsq.buf[rob.lsq_tag].valid = false;
            } else {
                nxt.reg.reg[rob.ins.rd] = rob.result;
            }
        }
        if (rob.ins.opcode == 0x23) {
            nxt.memory.data[rob.address] = rob.result & 0xFF;
            nxt.memory.data[rob.address + 1] = (rob.result >> 8) & 0xFF;
            nxt.memory.data[rob.address + 2] = (rob.result >> 16) & 0xFF;
            nxt.memory.data[rob.address + 3] = (rob.result >> 24) & 0xFF;
            nxt.lsq.buf[rob.lsq_tag].valid = false;
        }
    }
}