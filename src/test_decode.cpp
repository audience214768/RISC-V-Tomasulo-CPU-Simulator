// Unit tests for RISC-V instruction decode
// Covers all RV32I instruction formats from reference/reference-card.pdf
#include <cstdio>
#include <cstdlib>

#include "utils/config.hpp"
#include "utils/types.hpp"

// Forward-declare the decode function
auto decode(u32 raw) -> Instruction;

static int passed = 0, failed = 0;

static void check(const char* name, const char* field, u32 expected, u32 actual) {
    if (expected != actual) {
        fprintf(stderr, "FAIL %-20s %-6s: exp 0x%08x (%d) got 0x%08x (%d)\n",
                name, field, expected, (i32)expected, actual, (i32)actual);
        failed++;
    } else {
        passed++;
    }
}

// ─── Encode helpers (using RISC-V encoding from reference card) ───

static u32 R(u32 f7, u32 rs2, u32 rs1, u32 f3, u32 rd, u32 op) {
    return (f7 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}
static u32 I(u32 imm12, u32 rs1, u32 f3, u32 rd, u32 op) {
    return ((imm12 & 0xFFF) << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}
static u32 S(u32 imm, u32 rs2, u32 rs1, u32 f3, u32 op) {
    return ((imm & 0xFE0) << 20) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | ((imm & 0x1F) << 7) | op;
}
static u32 B(u32 off, u32 rs2, u32 rs1, u32 f3, u32 op) {
    u32 b12 = (off >> 12) & 1, b11 = (off >> 11) & 1;
    u32 b10_5 = (off >> 5) & 0x3F, b4_1 = (off >> 1) & 0xF;
    return (b12 << 31) | (b10_5 << 25) | (rs2 << 20) | (rs1 << 15)
         | (f3 << 12) | (b4_1 << 8) | (b11 << 7) | op;
}
static u32 U(u32 imm20, u32 rd, u32 op) {
    return (imm20 & 0xFFFFF000) | (rd << 7) | op;
}
static u32 J(u32 off, u32 rd, u32 op) {
    u32 b20 = (off >> 20) & 1, b10_1 = (off >> 1) & 0x3FF;
    u32 b11 = (off >> 11) & 1, b19_12 = (off >> 12) & 0xFF;
    return (b20 << 31) | (b10_1 << 21) | (b11 << 20) | (b19_12 << 12) | (rd << 7) | op;
}

int main() {
    Instruction ins;
    u32 raw;

    // ═══════════════════════════════════════════
    // R-type: opcode 0x33
    // ═══════════════════════════════════════════
    fprintf(stderr, "=== R-type (opcode 0x33) ===\n");

    raw = R(0x00, 3, 2, 0, 1, 0x33);
    ins = decode(raw);
    check("ADD", "op", 0x33, ins.opcode);
    check("ADD", "f3", 0, ins.func3);
    check("ADD", "f7", 0x00, ins.func7);
    check("ADD", "rd", 1, ins.rd);
    check("ADD", "rs1", 2, ins.rs1);
    check("ADD", "rs2", 3, ins.rs2);

    raw = R(0x20, 6, 5, 0, 4, 0x33);
    ins = decode(raw);
    check("SUB", "f7", 0x20, ins.func7);

    raw = R(0x00, 9, 8, 1, 7, 0x33);
    ins = decode(raw);
    check("SLL", "f3", 1, ins.func3);

    raw = R(0x00, 12, 11, 2, 10, 0x33);
    ins = decode(raw);
    check("SLT", "f3", 2, ins.func3);

    raw = R(0x00, 15, 14, 3, 13, 0x33);
    ins = decode(raw);
    check("SLTU", "f3", 3, ins.func3);

    raw = R(0x00, 18, 17, 4, 16, 0x33);
    ins = decode(raw);
    check("XOR", "f3", 4, ins.func3);

    raw = R(0x00, 21, 20, 5, 19, 0x33);
    ins = decode(raw);
    check("SRL", "f3", 5, ins.func3);
    check("SRL", "f7", 0x00, ins.func7);

    raw = R(0x20, 24, 23, 5, 22, 0x33);
    ins = decode(raw);
    check("SRA", "f3", 5, ins.func3);
    check("SRA", "f7", 0x20, ins.func7);

    raw = R(0x00, 27, 26, 6, 25, 0x33);
    ins = decode(raw);
    check("OR", "f3", 6, ins.func3);

    raw = R(0x00, 30, 29, 7, 28, 0x33);
    ins = decode(raw);
    check("AND", "f3", 7, ins.func3);

    // func7 7-bit mask verification
    raw = R(0x40, 3, 2, 0, 1, 0x33);
    ins = decode(raw);
    check("f7-0x40", "f7", 0x40, ins.func7);
    raw = R(0x7F, 3, 2, 0, 1, 0x33);
    ins = decode(raw);
    check("f7-0x7F", "f7", 0x7F, ins.func7);

    // ═══════════════════════════════════════════
    // I-type ALU: opcode 0x13
    // ═══════════════════════════════════════════
    fprintf(stderr, "=== I-type ALU (opcode 0x13) ===\n");

    raw = I(42, 2, 0, 1, 0x13);
    ins = decode(raw);
    check("ADDI+42", "imm", 42u, ins.imm);

    raw = I(0xFFF, 2, 0, 1, 0x13);
    ins = decode(raw);
    check("ADDI-1", "imm", (u32)-1, ins.imm);

    raw = I(0x7FF, 2, 0, 1, 0x13);
    ins = decode(raw);
    check("ADDI+2047", "imm", 2047u, ins.imm);

    raw = I(0x800, 2, 0, 1, 0x13);
    ins = decode(raw);
    check("ADDI-2048", "imm", (u32)-2048, ins.imm);

    raw = I(31, 2, 1, 1, 0x13);
    ins = decode(raw);
    check("SLLI", "imm", 31u, ins.imm);

    // SRAI: imm12 = (0x20 << 5) | 16
    raw = I(0x410, 2, 5, 1, 0x13);
    ins = decode(raw);
    check("SRAI", "f3", 5, ins.func3);
    check("SRAI", "imm", 1040u, ins.imm);

    // Sign-extend tests
    raw = I(0xABC, 2, 6, 1, 0x13);
    ins = decode(raw);
    check("ORI-sign", "imm", (u32)(i32)0xFFFFFABC, ins.imm);

    raw = I((-100) & 0xFFF, 2, 2, 1, 0x13);
    ins = decode(raw);
    check("SLTI-100", "imm", (u32)-100, ins.imm);

    // ═══════════════════════════════════════════
    // LOAD: opcode 0x03
    // ═══════════════════════════════════════════
    fprintf(stderr, "=== LOAD (opcode 0x03) ===\n");

    raw = I(42, 2, 0, 1, 0x03);
    ins = decode(raw);
    check("LB", "op", 0x03, ins.opcode);
    check("LB", "f3", 0, ins.func3);
    check("LB", "imm", 42u, ins.imm);

    raw = I(42, 2, 1, 1, 0x03);
    ins = decode(raw);
    check("LH", "f3", 1, ins.func3);

    raw = I(42, 2, 2, 1, 0x03);
    ins = decode(raw);
    check("LW", "f3", 2, ins.func3);

    raw = I(42, 2, 4, 1, 0x03);
    ins = decode(raw);
    check("LBU", "f3", 4, ins.func3);

    raw = I(42, 2, 5, 1, 0x03);
    ins = decode(raw);
    check("LHU", "f3", 5, ins.func3);

    raw = I(0x7FF, 2, 2, 1, 0x03);
    ins = decode(raw);
    check("LW+2047", "imm", 2047u, ins.imm);
    raw = I(0x800, 2, 2, 1, 0x03);
    ins = decode(raw);
    check("LW-2048", "imm", (u32)-2048, ins.imm);

    // ═══════════════════════════════════════════
    // STORE: opcode 0x23
    // ═══════════════════════════════════════════
    fprintf(stderr, "=== STORE (opcode 0x23) ===\n");

    raw = S(42, 1, 2, 0, 0x23);
    ins = decode(raw);
    check("SB", "op", 0x23, ins.opcode);
    check("SB", "f3", 0, ins.func3);
    check("SB", "rs1", 2u, ins.rs1);
    check("SB", "rs2", 1u, ins.rs2);
    check("SB", "imm", 42u, ins.imm);

    raw = S(0xFFF, 1, 2, 0, 0x23);
    ins = decode(raw);
    check("SB-1", "imm", (u32)-1, ins.imm);

    raw = S(0x7FF, 1, 2, 1, 0x23);
    ins = decode(raw);
    check("SH+2047", "imm", 2047u, ins.imm);

    raw = S(0x800, 1, 2, 1, 0x23);
    ins = decode(raw);
    check("SH-2048", "imm", (u32)-2048, ins.imm);

    raw = S(0xFE1, 1, 2, 2, 0x23);
    ins = decode(raw);
    check("SW-31", "imm", (u32)-31, ins.imm);

    raw = S(42, 1, 2, 2, 0x23);
    ins = decode(raw);
    check("SW", "f3", 2, ins.func3);

    // ═══════════════════════════════════════════
    // BRANCH: opcode 0x63
    // ═══════════════════════════════════════════
    fprintf(stderr, "=== BRANCH (opcode 0x63) ===\n");

    raw = B(16, 2, 1, 0, 0x63);
    ins = decode(raw);
    check("BEQ+16", "op", 0x63, ins.opcode);
    check("BEQ+16", "f3", 0, ins.func3);
    check("BEQ+16", "rs1", 1u, ins.rs1);
    check("BEQ+16", "rs2", 2u, ins.rs2);
    check("BEQ+16", "imm", 16u, ins.imm);

    raw = B((u32)-16, 2, 1, 1, 0x63);
    ins = decode(raw);
    check("BNE-16", "imm", (u32)-16, ins.imm);

    raw = B(4094, 2, 1, 4, 0x63);
    ins = decode(raw);
    check("BLT+4094", "f3", 4, ins.func3);
    check("BLT+4094", "imm", 4094u, ins.imm);

    raw = B((u32)-4096, 2, 1, 5, 0x63);
    ins = decode(raw);
    check("BGE-4096", "f3", 5, ins.func3);
    check("BGE-4096", "imm", (u32)-4096, ins.imm);

    raw = B(256, 2, 1, 6, 0x63);
    ins = decode(raw);
    check("BLTU", "f3", 6, ins.func3);

    raw = B((u32)-2, 2, 1, 7, 0x63);
    ins = decode(raw);
    check("BGEU-2", "f3", 7, ins.func3);
    check("BGEU-2", "imm", (u32)-2, ins.imm);

    raw = B(2, 2, 1, 0, 0x63);
    ins = decode(raw);
    check("BEQ+2", "imm", 2u, ins.imm);

    // ═══════════════════════════════════════════
    // JAL: opcode 0x6F
    // ═══════════════════════════════════════════
    fprintf(stderr, "=== JAL (opcode 0x6F) ===\n");

    // Real sample: 0x040010ef = jal ra, 0x1044 (PC=0x4, off=0x1040)
    raw = 0x040010ef;
    ins = decode(raw);
    check("JAL-s1", "op", 0x6F, ins.opcode);
    check("JAL-s1", "rd", 1u, ins.rd);
    check("JAL-s1", "imm", 0x1040u, ins.imm);

    // Real sample: 0xfb1ff0ef (off = 0x1000 - 0x1050 = -80)
    raw = 0xfb1ff0ef;
    ins = decode(raw);
    check("JAL-s2", "imm", (u32)-80, ins.imm);

    raw = J(4, 0, 0x6F);
    ins = decode(raw);
    check("JAL+4", "imm", 4u, ins.imm);

    raw = J(2048, 1, 0x6F);
    ins = decode(raw);
    check("JAL+2048", "imm", 2048u, ins.imm);

    raw = J((u32)-2048, 1, 0x6F);
    ins = decode(raw);
    check("JAL-2048", "imm", (u32)-2048, ins.imm);

    raw = J(1048574, 1, 0x6F);
    ins = decode(raw);
    check("JAL+1M", "imm", 1048574u, ins.imm);

    raw = J((u32)-1048576, 1, 0x6F);
    ins = decode(raw);
    check("JAL-1M", "imm", (u32)-1048576, ins.imm);

    // ═══════════════════════════════════════════
    // JALR: opcode 0x67
    // ═══════════════════════════════════════════
    fprintf(stderr, "=== JALR (opcode 0x67) ===\n");

    raw = I(42, 2, 0, 1, 0x67);
    ins = decode(raw);
    check("JALR", "op", 0x67, ins.opcode);
    check("JALR", "imm", 42u, ins.imm);

    raw = I(0, 1, 0, 0, 0x67); // ret
    ins = decode(raw);
    check("JALR-ret", "rd", 0u, ins.rd);
    check("JALR-ret", "rs1", 1u, ins.rs1);

    raw = I(0xFFF, 2, 0, 1, 0x67);
    ins = decode(raw);
    check("JALR-1", "imm", (u32)-1, ins.imm);

    // ═══════════════════════════════════════════
    // LUI: opcode 0x37
    // ═══════════════════════════════════════════
    fprintf(stderr, "=== LUI (opcode 0x37) ===\n");

    raw = U(0x20 << 12, 1, 0x37);
    ins = decode(raw);
    check("LUI", "op", 0x37, ins.opcode);
    check("LUI", "imm", 0x20000u, ins.imm);

    raw = U(0xFFFFF << 12, 31, 0x37);
    ins = decode(raw);
    check("LUI-max", "imm", 0xFFFFF000u, ins.imm);

    raw = 0x00020137; // lui sp, 0x20
    ins = decode(raw);
    check("LUI-samp", "rd", 2u, ins.rd);
    check("LUI-samp", "imm", 0x20000u, ins.imm);

    // ═══════════════════════════════════════════
    // AUIPC: opcode 0x17
    // ═══════════════════════════════════════════
    fprintf(stderr, "=== AUIPC (opcode 0x17) ===\n");

    raw = U(0x20 << 12, 1, 0x17);
    ins = decode(raw);
    check("AUIPC", "op", 0x17, ins.opcode);
    check("AUIPC", "imm", 0x20000u, ins.imm);

    // ═══════════════════════════════════════════
    // SYSTEM: opcode 0x73
    // ═══════════════════════════════════════════
    fprintf(stderr, "=== SYSTEM (opcode 0x73) ===\n");

    raw = 0x00000073;
    ins = decode(raw);
    check("ECALL", "op", 0x73, ins.opcode);
    check("ECALL", "imm", 0u, ins.imm);

    raw = 0x00100073;
    ins = decode(raw);
    check("EBREAK", "op", 0x73, ins.opcode);
    check("EBREAK", "imm", 1u, ins.imm);

    // ═══════════════════════════════════════════
    // FENCE: opcode 0x0F
    // ═══════════════════════════════════════════
    fprintf(stderr, "=== FENCE (opcode 0x0F) ===\n");
    raw = 0x0FF0000F;
    ins = decode(raw);
    check("FENCE", "op", 0x0F, ins.opcode);
    check("FENCE", "imm", 0u, ins.imm);

    // ═══════════════════════════════════════════
    // Register field extremes
    // ═══════════════════════════════════════════
    fprintf(stderr, "=== Register max values ===\n");
    raw = I(0, 31, 0, 31, 0x13);
    ins = decode(raw);
    check("reg", "rd", 31u, ins.rd);
    check("reg", "rs1", 31u, ins.rs1);

    raw = R(0, 31, 0, 0, 0, 0x33);
    ins = decode(raw);
    check("reg", "rs2", 31u, ins.rs2);

    // ═══════════════════════════════════════════
    fprintf(stderr, "\n=== %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
