#include "module/fetch.hpp"
#include <bit>
#include <array>

void fetch(const CPUState &cur, CPUState &nxt) {
    if (cur.fetch.halt) {
        return;
    }

    u32 addr;
    if (cur.fetch.mispredict) {
        addr = cur.fetch.correct_pc;
        nxt.fetch.mispredict = false;
    } else if (cur.fetch.pred_taken) {
        addr = cur.fetch.pred_target;
        nxt.fetch.pred_taken = false;
    } else {
        addr = cur.fetch.pc;
    }

    nxt.fetch.raw_instruction = std::bit_cast<u32>(std::array<u8, 4>{
        cur.memory.code[addr],
        cur.memory.code[addr + 1],
        cur.memory.code[addr + 2],
        cur.memory.code[addr + 3],
    });
    nxt.fetch.pc = addr + 4;
}