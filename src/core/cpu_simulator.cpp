#include "core/cpu_simulator.hpp"
#include "module/commit.hpp"
#include "module/execute.hpp"
#include "module/fetch.hpp"
#include "module/issue.hpp"
#include "module/memory.hpp"
#include "module/write_back.hpp"
#include "utils/types.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <sstream>

CPUSimulator::CPUSimulator() {
    std::string line;
    u32 address = 0;
    u32 offset = 0;
    while(std::getline(std::cin, line)) {
        if (line[0] == '@') {
            std::string str_addr = line.substr(1);
            address = std::stoul(str_addr, nullptr, 16);
            //std::cerr << address << std::endl;
            offset = 0;
        } else {
            std::stringstream ss(line);
            std::string str_byte;
            while (ss >> str_byte) {
                u8 data = static_cast<u8>(std::stoul(str_byte, nullptr, 16));
                state.memory.code[address + offset] = data;
                offset++;
            }
        }
    }
    memset(state.memory.data, 0, sizeof(state.memory.data));
    memset(state.reg.reg, 0, sizeof(state.reg.reg));
    state.rob.head = 0; state.rob.last = 0;
    state.rs.clear();
    state.rat.clear();
    state.lsq.clear();
    state.cdb.clear();
    state.fetch.pc = 0;
    state.fetch.halt = false;
    state.fetch.instruction_pc = 0;
    state.fetch.raw_instruction = 0;
    state.fetch.mispredict = false;
    state.fetch.correct_pc   = 0;
    state.fetch.pred_taken   = false;
    state.fetch.pred_target  = 0;
}

void CPUSimulator::run() {
    int ret;
    while (true) {
        if (state.fetch.halt && state.rob.empty()) {
            ret = state.reg.reg[10] & ((1 << 8) - 1);
            break;
        }
        clock++;
        tick();
        //fprintf(stderr, "pc = %08x\n", state.fetch.pc);
    }
    fprintf(stdout, "%d\n", ret);
}

void CPUSimulator::tick() {
    CPUState nxt = state;
    commit(state, nxt);
    //fprintf(stderr, "check1\n");
    writeBack(state, nxt);
    //fprintf(stderr, "check2\n");
    memory(state, nxt);
    //fprintf(stderr, "check3\n");
    issue(state, nxt);
    //fprintf(stderr, "check4\n");
    execute(state, nxt);
    //fprintf(stderr, "check5\n");
    fetch(state, nxt);
    nxt.reg.reg[0] = 0;
    state = nxt;
}