#include "storage/prf_module.hpp"

void PRFModule::eval(const WBPRFWritePorts &wb) {
    for (int i = 0; i < NUM_PHYS_REGS; i++) {
        values_[i].hold();
    }
    for (int j = 0; j < WBPRFWritePorts::kMaxWrites; j++) {
        if (wb.valid[j].read()) {
            values_[wb.preg[j].read()].next_raw() = wb.data[j].read();
        }
    }
}
