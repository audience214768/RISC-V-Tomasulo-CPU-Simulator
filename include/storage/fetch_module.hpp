#pragma once
#include "rtl/register.hpp"
#include "ports/fetch_ports.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <cstring>
#include <bit>
#include <array>

class FetchModule {
public:
    FetchModule() { reset(); }

    void drive_read_ports(FetchReadPorts &p) const {
        p.pc.write(pc_.cur());
        p.f2i_raw.write(f2i_raw_.cur());
        p.f2i_pc.write(f2i_pc_.cur());
        p.f2i_valid.write(f2i_valid_.cur());
        p.halt.write(halt_.cur());
    }

    void eval(const HaltRequestWritePorts  &halt_req,
              const ExecToFetchWritePorts  &exec,
              const IssueToFetchWritePorts &issue,
              const MemState              &mem) {
        // ---- baseline: hold all registers ----
        pc_.hold();
        f2i_raw_.hold();
        f2i_pc_.hold();
        f2i_valid_.hold();
        halt_.hold();

        // ---- P0 (highest): halt request ----
        if (halt_req.req.read()) {
            halt_.write(1);
        }
        if (halt_.cur()) {
            return;  // already halted, stop fetching
        }

        // ---- P1: execute mispredict redirect ----
        if (exec.mispredict.read()) {
            u32 addr = exec.correct_pc.read();
            u32 raw  = read_instruction(mem, addr);
            f2i_raw_.write(raw);
            f2i_pc_.write(addr);
            f2i_valid_.write(1);
            pc_.write(addr + 4);
            return;
        }

        // ---- P2: issue branch predict redirect ----
        if (issue.pred_taken.read()) {
            u32 addr = issue.pred_target.read();
            u32 raw  = read_instruction(mem, addr);
            f2i_raw_.write(raw);
            f2i_pc_.write(addr);
            f2i_valid_.write(1);
            pc_.write(addr + 4);
            return;
        }

        // ---- P3: issue structural stall ----
        if (issue.stall.read()) {
            // hold() keeps pc_ and f2i_ unchanged — instruction stays in F2I
            return;
        }

        // ---- P4: normal fetch ----
        u32 addr = pc_.cur();
        u32 raw  = read_instruction(mem, addr);
        f2i_raw_.write(raw);
        f2i_pc_.write(addr);
        f2i_valid_.write(1);
        pc_.write(addr + 4);
    }

    void tick() {
        pc_.tick();
        f2i_raw_.tick();
        f2i_pc_.tick();
        f2i_valid_.tick();
        halt_.tick();
    }

    void reset() {
        pc_.reset(0);
        f2i_raw_.reset(0);
        f2i_pc_.reset(0);
        f2i_valid_.reset(0);
        halt_.reset(0);
    }

private:
    static u32 read_instruction(const MemState &mem, u32 addr) {
        return std::bit_cast<u32>(
            std::array<u8, 4>{mem.buf[addr], mem.buf[addr + 1],
                              mem.buf[addr + 2], mem.buf[addr + 3]});
    }

    Register<32> pc_;
    Register<32> f2i_raw_;
    Register<32> f2i_pc_;
    Register<1>  f2i_valid_;
    Register<1>  halt_;
};
