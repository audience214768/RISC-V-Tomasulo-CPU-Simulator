#include "module/memory.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <cstdio>


void memory(const CPUState &cur, CPUState &nxt, const MemState &mem) {
    for (int i = 0; i < LSQ_SIZE; i++) {
        const LSQEntry &lsq = cur.lsq.buf[i];
        if (!lsq.valid) continue;
        if (!lsq.addr_ready) continue;
        if (!lsq.is_load) continue;
        if (lsq.data_ready) continue;

        // Check store-to-load ordering: any older unresolved store?
        bool addr_safe = true;
        int closest_store = -1;
        for (int j = cur.lsq.head; j != i; j = (j + 1) % LSQ_SIZE) {
            if (!cur.lsq.buf[j].valid) continue;
            if (cur.lsq.buf[j].is_load) continue;
            if (!cur.lsq.buf[j].addr_ready) { addr_safe = false; break; }
            if (!cur.lsq.buf[j].data_ready && cur.lsq.buf[j].addr == lsq.addr) {
                addr_safe = false; break;
            }
            if (cur.lsq.buf[j].addr == lsq.addr) closest_store = j;
        }
        if (!addr_safe) {
            nxt.lsq.buf[i].mem_wait = 0;  // reset wait on stall
            continue;
        }

        // Start or continue the memory latency countdown
        int &wait = nxt.lsq.buf[i].mem_wait;
        if (wait == 0) wait = MEM_LATENCY;
        wait--;
        if (wait > 0) continue;

        // Latency elapsed: perform the load
        u32 data = 0;
        if (closest_store >= 0) {
            data = cur.lsq.buf[closest_store].data;
        } else {
            u32 addr = lsq.addr;
            for (int j = 0; j < lsq.width; j++)
                data |= mem.buf[addr + j] << (8 * j);
            if (!lsq.is_unsigned && lsq.width < 4) {
                if (data & (1 << (8 * lsq.width - 1)))
                    data |= ~((1u << (8 * lsq.width)) - 1);
            }
        }
        nxt.cdb.push(lsq.rob_tag, data);
        nxt.lsq.buf[i].data = data;
        nxt.lsq.buf[i].data_ready = true;
    }
}