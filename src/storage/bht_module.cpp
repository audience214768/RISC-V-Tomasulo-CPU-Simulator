#include "storage/bht_module.hpp"

void BHTModule::eval(const ExecBHTWritePorts &exec) {
    for (int i = 0; i < BHT_SIZE; i++) {
        counters_[i].hold();
    }
    if (exec.update_req.read()) {
        u32 idx = exec.update_idx.read();
        u8 old = static_cast<u8>(counters_[idx].cur());
        if (exec.update_taken.read()) {
            counters_[idx].write(old < 3 ? static_cast<u8>(old + 1) : static_cast<u8>(3));
        } else {
            counters_[idx].write(old > 0 ? static_cast<u8>(old - 1) : static_cast<u8>(0));
        }
    }
}
