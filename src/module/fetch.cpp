#include "module/fetch.hpp"
#include <bit>
#include <array>

void fetch(const CPUState &cur, CPUState &nxt) {
    if (cur.fetch.halt) {
        return ;
    }
    u32 addr = cur.fetch.pc;
    nxt.fetch.raw_instruction = std::bit_cast<u32>(std::array<u8, 4>{
        cur.memory.code[addr],
        cur.memory.code[addr + 1],
        cur.memory.code[addr + 2],
        cur.memory.code[addr + 3],
    }); // LSB
    nxt.fetch.pc = cur.fetch.pc + 4;
}