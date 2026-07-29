#pragma once

#include "module/fetch.hpp"
#include "module/issue.hpp"
#include "module/execute.hpp"
#include "module/memory.hpp"
#include "module/write_back.hpp"
#include "module/commit.hpp"

#include "utils/types.hpp"
#include "utils/config.hpp"

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



