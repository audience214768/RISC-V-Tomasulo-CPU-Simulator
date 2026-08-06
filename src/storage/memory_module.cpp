#include "storage/memory_module.hpp"

void MemoryModule::start_refill(u32 addr) {
    refill_count++;
    u32 base = addr & ~(u32)kLineMask;
    size_t idx = index_of(base);
    if (lines_[idx].dirty.next_raw()) {
        wb_pending_ = true;
        wb_addr_ = (lines_[idx].tag.cur() << DCACHE_TAG_SHIFT) |
                   static_cast<u32>(idx << 5);
        for (int k = 0; k < DCACHE_LINE / 4; k++) {
            u32 w = 0;
            for (int b = 0; b < 4; b++)
                w |= (u32)lines_[idx].data[k * 4 + b].next_raw() << (8 * b);
            wb_data_[k] = w;
        }
    }
    lines_[idx].valid.next_raw() = 0;
    refill_state_.next_raw() = REFILL_LATENCY;
    refill_addr_.next_raw() = base;
}

void MemoryModule::eval(const MemWritePorts &store, const MemRefillReqWritePorts &req) {
    for (auto &l : lines_) {
        l.hold();
    }
    for (auto &e : store_buffer_) {
        e.hold();
    }
    store_buffer_count_.hold();
    refill_state_.hold();
    refill_addr_.hold();

    const size_t r_idx = index_of(refill_addr_.cur());
    const u32 r_tag = tag_of(refill_addr_.cur());

    // ---- Step 1: store commit (R2: before refill start; R4: before scan) ----
    bool pushed = false;
    if (store.valid.read()) {
        u32 store_addr = store.addr.read(), sdata = store.data.read();
        u8 sw = static_cast<u8>(store.width.read());
        if (hit(store_addr, sw)) {
            for (int b = 0; b < sw; b++) {
                size_t idx = index_of(store_addr + b);
                lines_[idx].data[(store_addr + b) & kLineMask].next_raw() = (sdata >> (8 * b)) & 0xFF;
                lines_[idx].dirty.next_raw() = 1;
            }
        } else if (store_buffer_count_.cur() < STORE_BUF_SIZE) {
            SBEntry &e = store_buffer_[store_buffer_count_.cur()];
            e.valid.next_raw() = 1;
            e.addr.next_raw() = store_addr;
            e.data.next_raw() = sdata;
            e.width.next_raw() = sw;
            store_buffer_count_.next_raw() = store_buffer_count_.cur() + 1;
            pushed = true;
        }
        // buffer full + miss cannot happen (eval_commit backpressures on
        // sb_full before driving the port); drop defensively.
    }

    // ---- Step 2: load-miss refill request (idle only) ----
    bool started = false;
    if (req.valid.read() && refill_state_.cur() == 0) {
        start_refill(req.addr.read());
        started = true;
    }

    // ---- Step 3: refill countdown / completion ----
    if (refill_state_.cur() > 1) {
        refill_state_.next_raw() = refill_state_.cur() - 1;
    } else if (refill_state_.cur() == 1) {
        u32 rbase = refill_addr_.cur();
        lines_[r_idx].valid.next_raw() = 1;
        lines_[r_idx].tag.next_raw() = r_tag;
        lines_[r_idx].dirty.next_raw() = 0;
        for (int k = 0; k < DCACHE_LINE; k++) {
            lines_[r_idx].data[k].next_raw() = buf_[rbase + k];
        }
        bool dirty = false;
        u32 ncount = store_buffer_count_.cur() + (pushed ? 1 : 0);
        for (int e = 0; e < (int)ncount; e++) {
            if (!store_buffer_[e].valid.next_raw()) continue;
            u32 eaddr = store_buffer_[e].addr.next_raw();
            u32 edata = store_buffer_[e].data.next_raw();
            u8 ewidth = static_cast<u8>(store_buffer_[e].width.next_raw());
            bool covers = false, all_valid = true;
            for (int b = 0; b < ewidth; b++) {
                u32 a = eaddr + b;
                if ((a & ~(u32)kLineMask) == rbase) {
                    covers = true;
                    lines_[r_idx].data[a & kLineMask].next_raw() = (edata >> (8 * b)) & 0xFF;
                    dirty = true;
                } else if (!line_hit(index_of(a), tag_of(a))) {
                    all_valid = false;   // other covering line not filled yet
                }
            }
            if (covers && all_valid) {
                store_buffer_[e].valid.next_raw() = 0;
            }
        }
        if (dirty) {
            lines_[r_idx].dirty.next_raw() = 1;
        }
        refill_state_.next_raw() = 0;
    }

    // ---- Step 4: drain the store miss buffer (idle only) ----
    if (refill_state_.cur() == 0 && !started && store_buffer_count_.cur() > 0 &&
        store_buffer_[0].valid.next_raw()) {
        u32 eaddr = store_buffer_[0].addr.next_raw();
        u32 edata = store_buffer_[0].data.next_raw();
        u8 ewidth = static_cast<u8>(store_buffer_[0].width.next_raw());
        if (hit(eaddr, ewidth)) {
            for (int b = 0; b < ewidth; b++) {
                size_t idx = index_of(eaddr + b);
                lines_[idx].data[(eaddr + b) & kLineMask].next_raw() = (edata >> (8 * b)) & 0xFF;
                lines_[idx].dirty.next_raw() = 1;
            }
            store_buffer_[0].valid.next_raw() = 0;
        } else {
            // miss: start a refill for the entry's first missing covering
            // line (the entry stays; the completion scan of step 3 merges it)
            u32 target = 0;
            for (int b = 0; b < ewidth; b++) {
                u32 a = eaddr + b;
                if (!line_hit(index_of(a), tag_of(a))) {
                    target = a & ~(u32)kLineMask;
                    break;
                }
            }
            start_refill(target);
        }
    }

    // ---- Compaction: shift valid entries to [0,count), preserving age
    // order (age order == merge order, so byte precedence stays correct) ----
    u32 ncount = store_buffer_count_.cur() + (pushed ? 1 : 0);
    u32 nc = 0;
    for (int e = 0; e < (int)ncount; e++) {
        if (!store_buffer_[e].valid.next_raw()) continue;
        if (e != (int)nc) {
            store_buffer_[nc].valid.next_raw() = 1;
            store_buffer_[nc].addr.next_raw() = store_buffer_[e].addr.next_raw();
            store_buffer_[nc].data.next_raw() = store_buffer_[e].data.next_raw();
            store_buffer_[nc].width.next_raw() = store_buffer_[e].width.next_raw();
        }
        nc++;
    }
    for (int e = (int)nc; e < STORE_BUF_SIZE; e++) {
        store_buffer_[e].valid.next_raw() = 0;
    }
    store_buffer_count_.next_raw() = nc;
}
