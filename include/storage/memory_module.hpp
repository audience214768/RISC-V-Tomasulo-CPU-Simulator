#pragma once
#include "rtl/register.hpp"
#include "ports/mem_ports.hpp"
#include "utils/config.hpp"
#include <cstring>
#include <bit>
#include <array>

class MemoryModule {
    struct Line {
        Register<1> valid, dirty;
        Register<32> tag;
        Register<8> data[DCACHE_LINE];
        void hold() {
            valid.hold(); dirty.hold(); tag.hold();
            for (int i = 0; i < DCACHE_LINE; i++) data[i].hold();
        }
        void tick() {
            valid.tick(); dirty.tick(); tag.tick();
            for (int i = 0; i < DCACHE_LINE; i++) data[i].tick();
        }
        void reset() {
            valid.reset(0); dirty.reset(0); tag.reset(0);
            for (int i = 0; i < DCACHE_LINE; i++) data[i].reset(0);
        }
    };
    struct SBEntry {
        Register<1> valid;
        Register<32> addr, data;
        Register<8> width;
        void hold() { valid.hold(); addr.hold(); data.hold(); width.hold(); }
        void tick() { valid.tick(); addr.tick(); data.tick(); width.tick(); }
        void reset() { valid.reset(0); addr.reset(0); data.reset(0); width.reset(0); }
    };

    u8 buf_[BUF_SIZE];       // main memory; written only at init and tick()
    Line lines_[DCACHE_LINES];
    SBEntry store_buffer_[STORE_BUF_SIZE];
    Register<8> store_buffer_count_;       // valid entries, always in slots [0,count)
    Register<8> refill_state_;   // 0 = idle, 1 = completes this cycle, else countdown
    Register<32> refill_addr_;   // line base of the refill target

    bool wb_pending_ = false;
    u32 wb_addr_ = 0;
    u32 wb_data_[DCACHE_LINE / 4] = {};

    static size_t index_of(u32 addr) { return (addr >> 5) & (DCACHE_LINES - 1); }
    static u32 tag_of(u32 addr) { return addr >> DCACHE_TAG_SHIFT; }

public:
    static constexpr size_t kLineMask = DCACHE_LINE - 1;
    static_assert((DCACHE_LINES & (DCACHE_LINES - 1)) == 0,
                  "DCACHE_LINES must be a power of two");

    size_t refill_count = 0;   // stats (non-RTL, incremented in start_refill)

    MemoryModule() { reset(); }

    bool hit(u32 addr, u8 width) const {
        for (int b = 0; b < width; b++) {
            if (!line_hit(index_of(addr + b), tag_of(addr + b))) return false;
        }
        return true;
    }
    u8 byte(u32 addr) const {
        return lines_[index_of(addr)].data[addr & kLineMask].cur();
    }
    u32 fetch_word(u32 addr) const {
        return std::bit_cast<u32>(
            std::array<u8, 4>{buf_[addr], buf_[addr + 1],
                              buf_[addr + 2], buf_[addr + 3]});
    }

    void write_init(u32 addr, u8 val) { buf_[addr] = val; }

    void drive_read_ports(MemReadPorts &p) const {
        p.refill_busy.write(refill_state_.cur() != 0 ? 1 : 0);
        p.sb_full.write(store_buffer_count_.cur() >= STORE_BUF_SIZE ? 1 : 0);
    }

    void eval(const MemWritePorts &store, const MemRefillReqWritePorts &req);

    void tick() {
        if (wb_pending_) {
            for (int k = 0; k < DCACHE_LINE / 4; k++) {
                u32 wd = wb_data_[k];
                for (int b = 0; b < 4; b++) {
                    buf_[wb_addr_ + k * 4 + b] = (wd >> (8 * b)) & 0xFF;
                }
            }
            wb_pending_ = false;
        }
        for (auto &l : lines_) l.tick();
        for (auto &e : store_buffer_) e.tick();
        store_buffer_count_.tick();
        refill_state_.tick();
        refill_addr_.tick();
    }
    void reset() {
        std::memset(buf_, 0, sizeof(buf_));
        wb_pending_ = false;
        for (auto &l : lines_) l.reset();
        for (auto &e : store_buffer_) e.reset();
        store_buffer_count_.reset(0);
        refill_state_.reset(0);
        refill_addr_.reset(0);
    }

private:
    bool line_hit(size_t idx, u32 tag) const {
        return lines_[idx].valid.cur() && lines_[idx].tag.cur() == tag;
    }
    void start_refill(u32 addr);
};
