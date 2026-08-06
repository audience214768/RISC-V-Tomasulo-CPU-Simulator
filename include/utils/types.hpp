#pragma once

#include <cstdlib>

#include "config.hpp"

using PhysRegNum = u32;
using ArchRegNum = u32;

struct Instruction {
    u32 raw;
    u32 opcode;
    u32 func3;
    u32 func7;
    ArchRegNum rd, rs1, rs2;
    u32 imm;
};








