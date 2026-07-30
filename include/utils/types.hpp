#pragma once

#include <cstdio>
#include <cstdlib>

#include "config.hpp"

using PhysRegNum = u32;
using ArchRegNum = u32;

struct Instruction {
    u32 raw;
    u32 opcode;
    u32 func3;
    u32 func7;
    ArchRegNum rd, rs1, rs2;
    u32 imm;
};

struct PRFState {
    u32 values[NUM_PHYS_REGS];
};

struct ReadyTableState {
    bool ready[NUM_PHYS_REGS];

    void reset() {
        for (int i = 0; i < NUM_ARCH_REGS; i++) ready[i] = true;
        for (int i = NUM_ARCH_REGS; i < NUM_PHYS_REGS; i++) ready[i] = false;
    }
};

struct FreeListState {
    PhysRegNum buf[NUM_PHYS_REGS];
    size_t head;
    size_t tail;
    size_t count;

    void reset() {
        head = 0;
        tail = 0;
        count = 0;
        for (u32 i = NUM_ARCH_REGS; i < NUM_PHYS_REGS; i++) {
            push(i);
        }
    }

    auto empty() const -> bool { return count == 0; }
    
    auto pop() -> PhysRegNum {
        if (empty()) {
            fprintf(stderr, "Free List Underflow!\n");
            exit(1);
        }
        PhysRegNum reg = buf[head];
        head = (head + 1) % NUM_PHYS_REGS;
        count--;
        return reg;
    }

    void push(PhysRegNum reg) {
        if (count == NUM_PHYS_REGS) {
            fprintf(stderr, "Free List Overflow!\n");
            exit(1);
        }
        buf[tail] = reg;
        tail = (tail + 1) % NUM_PHYS_REGS;
        count++;
    }
};

struct RATState {
    PhysRegNum map[NUM_ARCH_REGS];

    void reset() {
        for (u32 i = 0; i < NUM_ARCH_REGS; i++) {
            map[i] = i;
        }
    }
};

struct RSEntry {
    bool valid;
    Instruction ins;
    size_t rob_tag;
    size_t lsq_tag;

    PhysRegNum prs1;
    PhysRegNum prs2;
    PhysRegNum prd;

    u32 pc;
};

struct ROBEntry {
    bool ready;
    Instruction ins;

    ArchRegNum arch_dest;
    PhysRegNum new_pnum;
    PhysRegNum old_pnum;

    size_t lsq_tag;

};

struct CDBEntry {
    bool valid;
    PhysRegNum prd;
    u32 result;
    size_t rob_tag;
};

struct LSQEntry {
    bool valid;
    bool is_load;
    size_t rob_tag;

    bool addr_ready;
    u32 addr;

    bool data_ready;
    PhysRegNum prs2_or_prd;
    u32 data;

    u8 width;
    bool is_unsigned;
    int mem_wait;
};

struct MemState {
   u8 buf[BUF_SIZE];
};

struct FetchState {
    u32 pc;
    u32 instruction_pc;
    u32 raw_instruction;
    bool halt;
    bool mispredict;
    u32  correct_pc;
    bool pred_taken;
    u32  pred_target;
};

struct ROBState {
    ROBEntry buf[ROB_SIZE];
    size_t head;
    size_t last;
    auto empty() const -> bool { return head == last; }
    auto full() const -> bool { return (last + 1) % ROB_SIZE == head; }
};

struct RSState {
    RSEntry buf[RS_SIZE];
    void push(const RSEntry &rs) {
        auto find = false;
        for (int i = 0; i < RS_SIZE; i++) {
            if (!buf[i].valid) {
                buf[i] = rs;
                find = true;
                break;
            }
        }
        if (!find) {
            fprintf(stderr, "RS is full\n");
            exit(1);
        }
    }
    void clear() {
        for (int i = 0; i < RS_SIZE; i++) {
            buf[i].valid = false;
        }
    }
    auto full() const -> bool {
        for (int i = 0; i < RS_SIZE; i++) {
            if (!buf[i].valid) {
                return false;
            }
        }
        return true;
    }
};

struct CDBState {
    CDBEntry buf[CDB_SIZE];
    void clear() {
        for (int i = 0; i < CDB_SIZE; i++) {
            buf[i].valid = false;
        }
    }
    void push(PhysRegNum prd, u32 result, size_t rob_tag) {
        for (int j = 0; j < CDB_SIZE; j++) {
            if (!buf[j].valid) {
                buf[j].valid = true;
                buf[j].prd = prd;
                buf[j].result = result;
                buf[j].rob_tag = rob_tag;
                return;
            }
        }
        fprintf(stderr, "CDB FULL!\n");
        exit(1);
    }
};

struct LSQState {
    LSQEntry buf[LSQ_SIZE];
    size_t head;
    size_t last;

    auto full() const -> bool { return (last + 1) % LSQ_SIZE == head; }

    auto push(const LSQEntry &lsq) -> size_t {
        size_t ret = last;
        buf[last] = lsq;
        last = (last + 1) % LSQ_SIZE;
        return ret;
    }

    void clear() {
        head = last = 0;
        for (int i = 0; i < LSQ_SIZE; i++) {
            buf[i].valid = false;
        }
    }
};

struct CPUState {
    PRFState prf;
    ReadyTableState ready_table;
    FreeListState free_list;
    RATState rat;
    RATState arch_rat;
    
    FetchState fetch;
    ROBState rob;
    RSState rs;
    CDBState cdb;
    LSQState lsq;
};