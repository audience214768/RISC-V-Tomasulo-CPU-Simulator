#include "module/write_back.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <cstdio>

void writeBack(const CPUState &cur, CPUState &nxt) {
    for (int i = 0; i < CDB_SIZE; i++) {
        if (!cur.cdb.buf[i].valid) continue;

        PhysRegNum prd = cur.cdb.buf[i].prd;
        size_t rob_tag = cur.cdb.buf[i].rob_tag;
        u32 result = cur.cdb.buf[i].result;

        if (prd != 0) {
            nxt.prf.values[prd] = result;
            nxt.ready_table.ready[prd] = true;
        }

        if (rob_tag != NONE_ROB_TAG) {
            nxt.rob.buf[rob_tag].ready = true;
        }

        nxt.cdb.buf[i].valid = false;
    }
}