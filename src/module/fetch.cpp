#include "module/fetch.hpp"
#include <bit>
#include <array>

void fetch(const CPUState &cur, CPUState &nxt, const MemState &memory) {
    if (cur.fetch.halt) {
        return;
    }
    u32 addr;
    if (cur.fetch.mispredict) {
        addr = cur.fetch.correct_pc;
        nxt.fetch.mispredict = false;
    } else if (cur.fetch.pred_taken) {
        addr = cur.fetch.pred_target;
        //fprintf(stderr, "fetch pred_taken: target=0x%x\n", addr);
        nxt.fetch.pred_taken = false;
    } else {
        addr = cur.fetch.pc;
    }
    nxt.fetch.raw_instruction = std::bit_cast<u32>(std::array<u8, 4>{
        memory.buf[addr],
        memory.buf[addr + 1],
        memory.buf[addr + 2],
        memory.buf[addr + 3],
    });
    nxt.fetch.instruction_pc = addr;
    nxt.fetch.pc = addr + 4;
}