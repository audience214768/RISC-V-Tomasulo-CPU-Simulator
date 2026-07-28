#include "module/write_back.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"

void writeBack(const CPUState &cur, CPUState &nxt) {
    for (int i = 0; i < CDB_SIZE; i++) {
        if (cur.cdb.buf[i].valid) {
            //fprintf(stderr, "writeBack: cdb[%d] rob_tag=%zu result=0x%x\n",
            //        i, cur.cdb.buf[i].rob_tag, cur.cdb.buf[i].result);
            nxt.rob.buf[cur.cdb.buf[i].rob_tag].ready = true;
            nxt.rob.buf[cur.cdb.buf[i].rob_tag].result = cur.cdb.buf[i].result;
            nxt.cdb.buf[i].valid = false;
        }
    }
}