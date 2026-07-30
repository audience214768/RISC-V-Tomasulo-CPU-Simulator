#include "module/memory.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <cstdio>

void memory(const CPUState &cur, CPUState &nxt, const MemState &mem) {
    if (nxt.fetch.mispredict) return ;
    for (int i = 0; i < LSQ_SIZE; i++) {
        const LSQEntry &lsq = cur.lsq.buf[i];
        if (!lsq.valid) continue;
        if (!lsq.addr_ready) continue;
        if (!lsq.is_load) continue;
        if (lsq.data_ready) continue;

        bool addr_safe = true;
        int closest_store = -1;

        int j = (i - 1 + LSQ_SIZE) % LSQ_SIZE;
        //fprintf(stderr, "%zu %zu\n", cur.lsq.head, cur.lsq.last);
        while (i != cur.lsq.head) {
            //fprintf(stderr, "%d\n", j);
            const LSQEntry &store_cand = cur.lsq.buf[j];
            if (store_cand.valid && !store_cand.is_load) {
                if (!store_cand.addr_ready) {
                    //if (cur.rob.buf[lsq.rob_tag].ins.raw == 0x06c72783) fprintf(stderr, "%0x %d %d\n", cur.rob.buf[cur.lsq.buf[j].rob_tag].ins.raw, i, j);
                    addr_safe = false;
                    break;
                }

                if (store_cand.addr == lsq.addr) {
                    closest_store = j;
                    if (!store_cand.data_ready) {
                        addr_safe = false;
                    }
                    break; 
                }
            }

            if (j == cur.lsq.head) break;
            j = (j - 1 + LSQ_SIZE) % LSQ_SIZE;
        }
        //if (cur.rob.buf[lsq.rob_tag].ins.raw == 0x06c72783) fprintf(stderr, "check1\n");
        if (!addr_safe) {
            nxt.lsq.buf[i].mem_wait = 0;
            continue;
        }
        int &wait = nxt.lsq.buf[i].mem_wait;
        if (wait == 0) wait = MEM_LATENCY;
        wait--;
        //if (cur.rob.buf[lsq.rob_tag].ins.raw == 0x06c72783) fprintf(stderr, "check %d\n", wait);
        if (wait > 0) continue;

        u32 data = 0;
        if (closest_store >= 0) { //remember partital overlap
            data = cur.lsq.buf[closest_store].data;
        } else {
            u32 addr = lsq.addr;
            for (int j = 0; j < lsq.width; j++) {
                data |= static_cast<u32>(mem.buf[addr + j]) << (8 * j);
            }

            if (!lsq.is_unsigned && lsq.width < 4) {
                if (data & (1u << (8 * lsq.width - 1))) {
                    data |= ~((1u << (8 * lsq.width)) - 1);
                }
            }
        }

        PhysRegNum prd = lsq.prs2_or_prd;

        if (prd != 0) {
            nxt.cdb.push(prd, data, lsq.rob_tag);
        } else {
            nxt.rob.buf[lsq.rob_tag].ready = true;
        }

        nxt.lsq.buf[i].data = data;
        nxt.lsq.buf[i].data_ready = true;
        nxt.lsq.buf[i].valid = false;
    }
}