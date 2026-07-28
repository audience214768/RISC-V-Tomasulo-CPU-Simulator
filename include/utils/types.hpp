#pragma once

#include <cstdio>
#include <cstdlib>
#include <utility>

#include "config.hpp"

struct Instruction {
    u32 raw;
    u32 opcode;
    u32 func3;
    u32 func7;
    u32 rd, rs1, rs2;
    u32 imm;
};

struct LSQEntry {
    bool valid;
    bool is_load;
    size_t rob_tag;

    bool addr_ready;
    u32 addr;

    bool data_ready;
    u32 data;

    u8 width;
    bool is_unsigned;
};

struct ROBEntry {
    bool ready;  //wait for commit
    Instruction ins;
    u32 result;

    //store
    u32 address;
    size_t lsq_tag;
};

struct RSEntry {
    bool valid; // not rubbish data
    Instruction ins;
    size_t rob_tag;
    size_t lsq_tag;

    //rs1
    bool ready1;
    u32 value1;
    size_t query1;

    //rs2
    bool ready2;
    u32 value2;
    size_t query2;

    u32 pc;
};

struct CDBEntry {
    bool valid;
    size_t rob_tag;
    u32 result;
};

struct MemState {
   u8 data[BUF_SIZE];
   u8 code[BUF_SIZE];
};

struct RegState {
    u32 reg[32];
};

struct FetchState {
    u32 pc;
    u32 instruction_pc;
    u32 raw_instruction;
    bool halt;
    bool mispredict;
    u32  correct_pc;   // mispredict recovery target (execute writes)
    bool pred_taken;   // JAL redirect (issue writes, fetch reads next cycle)
    u32  pred_target;  // JAL target PC (issue writes)
};

struct ROBState {
    ROBEntry buf[ROB_SIZE];
    size_t head;
    size_t last;
    auto empty() const  -> bool { return head == last; }
    auto full() const -> bool { return (last + 1) % ROB_SIZE == head; }
};

struct RATState {
    size_t map[32];
    void clear() {
        for (int i = 0; i < 32; i++) {
            map[i] = NONE_ROB_TAG;
        }
    }
};

struct RSState {
    RSEntry buf[RS_SIZE];
    void push(const RSEntry &rs) {
        //fprintf(stderr, "rs push\n");
        auto find = false;
        for (int i = 0; i < RS_SIZE; i++) {
            if (!buf[i].valid) {
                buf[i] = std::move(rs);
                find = true;
                break;
            }
        }
        if (!find) {
            fprintf(stderr, "rs is full\n");
            exit(1);
        }
    }
    void clear() {
        for (int i = 0; i < RS_SIZE; i++) {
            buf[i].valid = false;
        }
    }
};

struct CDBState {
    CDBEntry buf[CDB_SIZE];
    void clear() {
        for (int i = 0; i < CDB_SIZE; i++) {
            buf[i].valid = false;
            buf[i].rob_tag = NONE_ROB_TAG;
        }
    }
    void push(size_t rob_tag, u32 result) {
        //fprintf(stderr, "cdb push\n");
        for (int j = 0; j < CDB_SIZE; j++) {
            if (!buf[j].valid) {
                buf[j].valid = true;
                buf[j].rob_tag = rob_tag;
                buf[j].result = result;
                break;
            }
        }
    }
};

struct LSQState {
    LSQEntry buf[LSQ_SIZE];
    size_t head;
    size_t last;

    auto push(const LSQEntry &lsq) -> size_t {
        //fprintf(stderr, "lsq push\n");
        if ((last + 1) % LSQ_SIZE == head) {
            fprintf(stderr, "LSQ full\n");
            exit(1);
        }
        int ret = last;
        buf[last] = std::move(lsq);
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
    MemState memory;
    RegState reg;
    FetchState fetch;
    ROBState rob;
    RSState rs;
    RATState rat;
    CDBState cdb;
    LSQState lsq;
};





