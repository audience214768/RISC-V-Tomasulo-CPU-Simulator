#include "module/memory.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <cstdio>


void memory(const CPUState &cur, CPUState &nxt) {
    for (int i = 0; i < LSQ_SIZE; i++) {
        const LSQEntry &lsq = cur.lsq.buf[i];
        if (!lsq.valid) continue;
        //fprintf(stderr, "lsq state valid=%d addr_ready=%d is_load=%d\n", lsq.valid, lsq.addr_ready, lsq.is_load);
        if (!lsq.addr_ready) continue;
        if (!lsq.is_load) continue;
        if (lsq.data_ready) continue;
        bool addr_safe = true;
        bool data_transfer = false;
        u32 data = 0;
        int closest_store = -1;
        for (int j = cur.lsq.head; j != i; j = (j + 1) % LSQ_SIZE) {
            if (!cur.lsq.buf[j].valid) continue;
            if (cur.lsq.buf[j].is_load) continue;
            if (!cur.lsq.buf[j].addr_ready) {
                addr_safe = false;
                break;
            }
            if (cur.lsq.buf[j].addr != lsq.addr) continue;
            if (!cur.lsq.buf[j].data_ready) {
                addr_safe = false;
                break;
            }
            closest_store = j;
        }
        if (!addr_safe) {
            //fprintf(stderr, "mem stall: rob=%zu addr=0x%x\n", lsq.rob_tag, lsq.addr);
            continue;
        }
        if (closest_store >= 0) {
            data_transfer = true;
            data = cur.lsq.buf[closest_store].data;
        }
        if (data_transfer) {
            //fprintf(stderr, "mem fwd: rob=%zu addr=0x%x data=%d\n", lsq.rob_tag, lsq.addr, data);
            nxt.cdb.push(lsq.rob_tag, data);
        } else {
            u32 addr = lsq.addr;
            for (int j = 0; j < lsq.width; j++) {
                data |= cur.memory.buf[addr + j] << (8 * j);
            }
            //if (addr == 0x1ff4c) fprintf(stderr, "LD sp+12: data=%d\n", data);
            //fprintf(stderr, "mem read: rob=%zu addr=0x%x data=%d\n", lsq.rob_tag, lsq.addr, data);
            if (!lsq.is_unsigned && lsq.width < 4) {
                if (data & (1 << (8 * lsq.width - 1))) {
                    data |= ~((1u << (8 * lsq.width)) - 1);
                }
            }
            //fprintf(stderr, "mem read: rob=%zu addr=0x%x data=%d\n", lsq.rob_tag, lsq.addr, data);
            nxt.cdb.push(lsq.rob_tag, data);
        }
        nxt.lsq.buf[i].data = data;
        nxt.lsq.buf[i].data_ready = true;
    }
}