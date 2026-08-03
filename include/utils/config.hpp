#pragma once

#include <cstddef>
#include <cstdint>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;

constexpr size_t ROB_SIZE = 32;
constexpr size_t RS_SIZE = 12;
constexpr size_t LSQ_SIZE = 18;
constexpr size_t CDB_SIZE = 2;
constexpr size_t BUF_SIZE = 256 * 1024;
constexpr size_t BHT_SIZE = 32;
constexpr size_t RAS_SIZE = 32;

constexpr size_t NONE_ROB_TAG = ROB_SIZE + 1;

constexpr u32 TERMINATE_INST = 0x0ff00513;

constexpr int MEM_LATENCY = 3;

constexpr int NUM_ARCH_REGS = 32;
constexpr int NUM_PHYS_REGS = 64;

