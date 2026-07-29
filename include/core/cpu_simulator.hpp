#pragma once
#include "utils/types.hpp"

class CPUSimulator {
private:
    CPUState state;
    MemState mem;
    size_t clock;
    void tick();
public:
    CPUSimulator();
    void run();
};



