#pragma once
#include "rtl/register.hpp"
#include "ports/fetch_ports.hpp"
#include "ports/bht_ports.hpp"
#include "ports/ras_ports.hpp"
#include "storage/memory_module.hpp"
#include "utils/config.hpp"

class FetchModule {
public:
    FetchModule() { reset(); }

    void drive_read_ports(FetchReadPorts &p) const {
        p.pc.write(pc_.cur());
        p.f2i_raw.write(f2i_raw_.cur());
        p.f2i_pc.write(f2i_pc_.cur());
        p.f2i_valid.write(f2i_valid_.cur());
        p.f2i_pred.write(f2i_pred_.cur());
        p.f2i_pred_target.write(f2i_pred_target_.cur());
        p.f2i_ras_snap.write(f2i_ras_snap_.cur());
        p.halt.write(halt_.cur());
    }

    void eval(
        const HaltRequestWritePorts &halt_req,
        const ExecToFetchWritePorts &exec,
        const IssueToFetchWritePorts &issue,
        const BHTReadPorts &bht,
        const RASReadPorts &ras,
        const FetchRASWritePorts &ras_fetch,
        const MemoryModule &mem
    );

    void tick() {
        pc_.tick();
        f2i_raw_.tick();
        f2i_pc_.tick();
        f2i_valid_.tick();
        f2i_pred_.tick();
        f2i_pred_target_.tick();
        f2i_ras_snap_.tick();
        halt_.tick();
    }

    void reset() {
        pc_.reset(0);
        f2i_raw_.reset(0);
        f2i_pc_.reset(0);
        f2i_valid_.reset(0);
        f2i_pred_.reset(0);
        f2i_pred_target_.reset(0);
        f2i_ras_snap_.reset(0);
        halt_.reset(0);
    }

private:
    static u32 read_instruction(const MemoryModule &mem, u32 addr) {
        return mem.fetch_word(addr);
    }

    Register<32> pc_;
    Register<32> f2i_raw_;
    Register<32> f2i_pc_;
    Register<1>  f2i_valid_;
    Register<1>  f2i_pred_;
    Register<32> f2i_pred_target_;
    Register<32> f2i_ras_snap_;   // RAS head at branch fetch (mispredict restore)
    Register<1>  halt_;
};
