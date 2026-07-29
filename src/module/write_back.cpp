#include "module/write_back.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"

void writeBack(const CPUState &cur, CPUState &nxt) {
    for (int i = 0; i < CDB_SIZE; i++) {
        if (cur.cdb.buf[i].valid) {
            size_t tag = cur.cdb.buf[i].rob_tag;
            u32 result = cur.cdb.buf[i].result;
            nxt.rob.buf[tag].ready = true;
            nxt.rob.buf[tag].result = result;

            for (int j = 0; j < RS_SIZE; j++) {
                if (cur.rs.buf[j].valid && !cur.rs.buf[j].ready1
                    && cur.rs.buf[j].query1 == tag) {
                    nxt.rs.buf[j].ready1 = true;
                    nxt.rs.buf[j].value1 = result;
                }
                if (cur.rs.buf[j].valid && !cur.rs.buf[j].ready2
                    && cur.rs.buf[j].query2 == tag) {
                    nxt.rs.buf[j].ready2 = true;
                    nxt.rs.buf[j].value2 = result;
                }
            }

            nxt.cdb.buf[i].valid = false;
        }
    }
}