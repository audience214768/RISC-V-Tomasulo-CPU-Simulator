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
            offset = 0;
        } else {
            std::stringstream ss(line);
            std::string str_byte;
            while (ss >> str_byte) {
                u8 data = static_cast<u8>(std::stoul(str_byte, nullptr, 16));
                state.memory.code[address + offset] = data;
            }
        }
    }
    memset(state.memory.data, 0, sizeof(state.memory.data));
    memset(state.reg.reg, 0, sizeof(state.reg.reg));
    state.rob.head = 0; state.rob.last = 0;
    state.rs.clear();
    state.rat.clear();
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
    }
    fprintf(stdout, "%d\n", ret);
}

void CPUSimulator::tick() {
    CPUState nxt = state;
    commit(state, nxt);
    writeBack(state, nxt);
    memory(state, nxt);
    execute(state, nxt);
    issue(state, nxt);
    fetch(state, nxt);
}