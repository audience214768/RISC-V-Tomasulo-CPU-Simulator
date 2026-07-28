#pragma once

#include "utils/types.hpp"

auto decode(u32 raw) -> Instruction;

void issue(const CPUState &cur, CPUState &nxt);
