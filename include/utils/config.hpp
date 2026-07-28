#pragma once

#include <cstddef>
#include <cstdint>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;

constexpr size_t ROB_SIZE = 8;
constexpr size_t RS_SIZE = 8;
constexpr size_t LSQ_SIZE = 8;
constexpr size_t CDB_SIZE = 4;
constexpr size_t BUF_SIZE = 256 * 1024;
constexpr size_t BHT_SIZE = 64;

constexpr size_t NONE_ROB_TAG = ROB_SIZE + 1;

constexpr u32 TERMINATE_INST = 0x0ff00513;

