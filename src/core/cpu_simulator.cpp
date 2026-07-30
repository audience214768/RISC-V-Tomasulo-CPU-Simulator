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
    
    memset(mem.buf, 0, sizeof(mem.buf));
    memset(state.prf.values, 0, sizeof(state.prf.values));
    
    state.rat.reset();
    state.arch_rat.reset();
    state.free_list.reset();
    state.ready_table.reset();

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        if (line[0] == '@') {
            std::string str_addr = line.substr(1);
            address = std::stoul(str_addr, nullptr, 16);
            offset = 0;
        } else {
            std::stringstream ss(line);
            std::string str_byte;
            while (ss >> str_byte) {
                u8 data = static_cast<u8>(std::stoul(str_byte, nullptr, 16));
                mem.buf[address + offset] = data;
                offset++;
            }
        }
    }

    state.rob.head = 0; 
    state.rob.last = 0;
    state.rs.clear();
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

    clock = 0;
}

extern size_t branch_count;
extern size_t mispredict_count;

void CPUSimulator::run() {
    int ret = 0;
    while (true) {
        if (state.fetch.halt && state.rob.empty()) {
            PhysRegNum pr10 = state.rat.map[10];
            u32 r10_value = state.prf.values[pr10];
            ret = r10_value & ((1 << 8) - 1);
            break;
        }
        clock++;
        tick();
        // fprintf(stderr, "clock = %zu pc = 0x%08x\n", clock, state.fetch.pc);
    }
    fprintf(stdout, "%d\n", ret);
    if (branch_count > 0) {
        fprintf(stderr, "clock = %zu total_branch = %zu predict_success = %zu ratio = %0.2f\n",
                clock, branch_count, branch_count - mispredict_count,
                static_cast<double>(branch_count - mispredict_count) / branch_count);
    }
}

void CPUSimulator::tick() {
    CPUState nxt = state;

    //fprintf(stderr, "check0\n");
    commit(state, nxt, mem);
    //fprintf(stderr, "check1\n");
    writeBack(state, nxt);
    //fprintf(stderr, "check2\n");
    memory(state, nxt, mem);
    //fprintf(stderr, "check3\n");
    issue(state, nxt);
    //fprintf(stderr, "check4\n");
    execute(state, nxt);
    //fprintf(stderr, "check5\n");
    fetch(state, nxt, mem);
    //fprintf(stderr, "check6\n");
    PhysRegNum p0 = nxt.rat.map[0];
    nxt.prf.values[p0] = 0;
    nxt.ready_table.ready[p0] = true;

    state = nxt;
}