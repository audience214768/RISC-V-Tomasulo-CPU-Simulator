#include "module/memory.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <array>


void memory_access(const CPUState &cur, CPUState &nxt) {
    for (int i = 0; i < LSQ_SIZE; i++) {
        const LSQEntry &lsq = cur.lsq.buf[i];
        if (!lsq.valid) continue;
        if (!lsq.addr_ready) continue;
        bool addr_safe = true;
        bool data_transfer = false;
        u32 data;
        int j = i;
        do {
            j = (j - 1 + LSQ_SIZE) % LSQ_SIZE;
            if (cur.lsq.buf[j].is_load) continue;
            if (cur.lsq.buf[j].addr_ready && cur.lsq.buf[j].addr != lsq.addr) continue;
            if (cur.lsq.buf[j].addr_ready && cur.lsq.buf[j].data_ready) {
                data_transfer = true;
                data = cur.lsq.buf[j].data;
                break;
            }
            addr_safe = false;
            break;
        } while (j != cur.lsq.head);
        if (!addr_safe) continue;
        if (data_transfer) {
            nxt.cdb.push(lsq.rob_tag, data);
        } else {
            u32 addr = lsq.addr;
            for (int j = 0; j < lsq.width; j++) {
                data |= cur.memory.data[addr + j] << (8 * j);
            }
            if (!lsq.is_unsigned) {
                if (data & (1 << (8 * lsq.width - 1))) {
                    data |= ~((1u << 8 * lsq.width) - 1);
                }
            }
            nxt.cdb.push(lsq.rob_tag, data);
        }
    }
}