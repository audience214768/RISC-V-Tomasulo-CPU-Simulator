#include "module/commit.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <cstdio>

void commit(const CPUState &cur, CPUState &nxt, MemState &memory) {
    while (nxt.rob.head != cur.rob.last && cur.rob.buf[nxt.rob.head].ready) {
        size_t commit_tag = nxt.rob.head;
        const ROBEntry &rob = cur.rob.buf[commit_tag];
        nxt.rob.head = (nxt.rob.head + 1) % ROB_SIZE;

        u8 opcode = rob.ins.opcode;
        u8 rd     = rob.ins.rd;

        if (rd != 0 && (
            opcode == 0x33 || // ALU_R
            opcode == 0x13 || // ALU_I
            opcode == 0x03 || // Load
            opcode == 0x6F || // JAL
            opcode == 0x67 || // JALR
            opcode == 0x17 || // AUIPC
            opcode == 0x37    // LUI
        )) {
            if (rob.new_pnum) {
                nxt.free_list.push(rob.old_pnum);
            }
        }

        if (opcode == 0x23) {
            size_t lsq_idx = rob.lsq_tag;
            const LSQEntry &lsq = cur.lsq.buf[lsq_idx];

            u32 addr = lsq.addr;
            u32 data = lsq.data;
            int width = lsq.width;

            for (int w = 0; w < width; w++) {
                memory.buf[addr + w] = (data >> (8 * w)) & 0xFF;
            }

            nxt.lsq.buf[lsq_idx].valid = false;
        }
    }
    while (nxt.lsq.head != nxt.lsq.last && !nxt.lsq.buf[nxt.lsq.head].valid) {
        nxt.lsq.head = (nxt.lsq.head + 1) % LSQ_SIZE;
    }
}