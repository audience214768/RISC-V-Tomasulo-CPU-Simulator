#include "core/cpu_top.hpp"
#include "utils/config.hpp"
#include "utils/types.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <bit>
#include <array>
#include <string>
#include <iostream>
#include <sstream>

TomasuloTop::TomasuloTop() {
    std::memset(mem_.buf, 0, sizeof(mem_.buf));
    prf_.reset(); ready_table_.reset(); rat_.reset(); free_list_.reset();
    cdb_.reset(); rs_.reset(); rob_.reset(); lsq_.reset();
    std::memset(bht_, 1, sizeof(bht_));           // weak not-taken
    std::memset(bht_pred_, 0, sizeof(bht_pred_));
    load_memory();
}

void TomasuloTop::load_memory() {
    std::string line; u32 a = 0, o = 0;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        if (line[0] == '@') { 
            a = std::stoul(line.substr(1), nullptr, 16); 
            o = 0; 
        }
        else { 
            std::stringstream ss(line); 
            std::string sb;
            while (ss >> sb) { 
                mem_.buf[a + o] = static_cast<u8>(std::stoul(sb, nullptr, 16)); 
                o++; 
            } 
        }
    }
}

auto TomasuloTop::sign_extend(u32 v, u8 b) -> u32 {
    if (v & (1u << (b - 1))) return v | (~((1u << b) - 1)); 
    return v & ((1u << b) - 1);
}
auto TomasuloTop::decode(u32 raw) -> Instruction {
    u32 op = raw & 0x7f, imm = 0;
    switch (op) {
        case 0x03: case 0x13: case 0x67: case 0x73: 
            imm = sign_extend((raw >> 20) & 0xFFF, 12); 
            break;
        case 0x23: 
            imm = sign_extend(((raw >> 20) & 0xFE0) | ((raw >> 7) & 0x1F), 12); 
            break;
        case 0x63: 
            imm = sign_extend(((raw >> 19) & 0x1000) | ((raw << 4) & 0x800) | ((raw >> 20) & 0x7E0) | ((raw >> 7) & 0x1E), 13); 
            break;
        case 0x37: case 0x17: 
            imm = raw & 0xFFFFF000; 
            break;
        case 0x6F: 
            imm = sign_extend(((raw >> 11) & 0x100000) | (raw & 0xFF000) | ((raw >> 9) & 0x800) | ((raw >> 20) & 0x7FE), 21); 
            break;
        default: imm = 0;
    }
    return Instruction{
        raw, op, (raw >> 12) & 7, (raw >> 25) & 0x7F,
        (ArchRegNum)((raw >> 7) & 0x1F), (ArchRegNum)((raw >> 15) & 0x1F), 
        (ArchRegNum)((raw >> 20) & 0x1F), imm
    };
}
auto TomasuloTop::ALU_R(u32 f3, u32 f7, u32 r1, u32 r2) -> u32 {
    switch (f3) {
        case 0x0: return f7 ? r1 - r2 : r1 + r2;
        case 0x1: return r1 << (r2 & 0x1F);
        case 0x2: return (i32)r1 < (i32)r2;
        case 0x3: return r1 < r2;
        case 0x4: return r1 ^ r2;
        case 0x5: return f7 ? (u32)((i32)r1 >> (r2 & 0x1F)) : r1 >> (r2 & 0x1F);
        case 0x6: return r1 | r2;
        case 0x7: return r1 & r2;
        default: exit(1);
    }
}
auto TomasuloTop::ALU_I(u32 f3, u32 f7, u32 r1, u32 imm) -> u32 {
    switch (f3) {
        case 0x0: return r1 + imm;
        case 0x1: return r1 << (imm & 0x1F);
        case 0x2: return (i32)r1 < (i32)imm;
        case 0x3: return r1 < imm;
        case 0x4: return r1 ^ imm;
        case 0x5: return f7 ? (u32)((i32)r1 >> (imm & 0x1F)) : r1 >> (imm & 0x1F);
        case 0x6: return r1 | imm;
        case 0x7: return r1 & imm;
        default: exit(1);
    }
}
auto TomasuloTop::branch_cond(u32 f3, u32 r1, u32 r2) -> bool {
    switch (f3) {
        case 0x0: return r1 == r2;
        case 0x1: return r1 != r2;
        case 0x4: return (i32)r1 < (i32)r2;
        case 0x5: return (i32)r1 >= (i32)r2;
        case 0x6: return r1 < r2;
        case 0x7: return r1 >= r2;
        default: exit(1);
    }
}

static bool fetch_op_wire(const CDBReadPorts &cdb, const ReadyTableReadPorts &rt,
                          const PRFReadPorts &prf, u32 prs, u32 &out) {
    if (prs == 0) { 
        out = 0; 
        return true; 
    }
    for (int c = 0; c < CDB_SIZE; c++) {
        if (cdb.valid[c].read() && cdb.prd[c].read() == prs) { 
            out = cdb.result[c].read(); 
            return true; 
        }
    }
    if (rt.ready[prs].read()) { 
        out = prf.data[prs].read(); return true; 
    }
    out = 0; 
    return false;
}

void TomasuloTop::flush_pipeline(size_t branch_rob_tag) {
    size_t fs = (branch_rob_tag + 1) % ROB_SIZE;
    size_t fe = rob_rp_.last.read();

    // Suppress this cycle's issue outputs. Flush only gates issue/fetch signals;
    // commit/memory/writeback signals (from pre-branch instructions) are unaffected.
    issue_lsq_.suppressed.write(1);
    issue_rs_.suppressed.write(1);
    issue_fl_.suppressed.write(1);
    issue_rat_.suppressed.write(1);
    issue_ready_.suppressed.write(1);
    flush_rob_.set_last_valid.write(1);
    flush_rob_.set_last_val.write(static_cast<u32>(fs));

    if (fs == fe) {
        return ;
    }

    u8 fc = 0, ffc = 0;
    size_t curr = (fe + ROB_SIZE - 1) % ROB_SIZE;
    //fprintf(stderr, "flush: head = %d last = %d branch_rob_tag : %zu\n", rob_rp_.head.read(), rob_rp_.last.read(), branch_rob_tag);
    while (curr != branch_rob_tag) {
        //fprintf(stderr, "%zu ", curr);
        u32 opcode = rob_rp_.opcode[curr].read(), rd = rob_rp_.rd[curr].read();
        u32 np = rob_rp_.new_pnum[curr].read(), op = rob_rp_.old_pnum[curr].read();
        //fprintf(stderr, "%0x %d\n", rob_rp_.ins_raw[curr].read(), np);
        if (rd != 0 && np != 0) {
            flush_rat_.restore_rd[fc].write(rd); 
            flush_rat_.restore_old[fc].write(op); 
            fc++;
            flush_fl_.push_pregs[ffc].write(np); 
            ffc++;
            flush_ready_.clear_req[np].write(1);
            //if (np == 41) fprintf(stderr, "flush the 41\n");
        }
        if (curr == fs) break;
        curr = (curr + ROB_SIZE - 1) % ROB_SIZE;
    }
    flush_rat_.restore_count.write(fc);
    flush_fl_.push_count.write(ffc);

    auto in_range = [&](size_t tag) -> bool {
        //fprintf(stderr, "flush range fs = %d fe = %d tag = %d\n", fs, fe, tag);
        if (fs <= fe) {
            return tag >= fs && tag < fe; 
        } else {
            return tag >= fs || tag < fe; 
        }
    };
    for (int i = 0; i < RS_SIZE; i++) {
        if (rs_rp_.valid[i].read() && in_range(rs_rp_.rob_tag[i].read())) {
            exec_rs_.flush_mask[i].write(1);
        }
    }
    for (int i = 0; i < LSQ_SIZE; i++) {
        if (lsq_rp_.valid[i].read() && in_range(lsq_rp_.rob_tag[i].read())) {
            exec_lsq_.flush_mask[i].write(1);
        }
    }
}

void TomasuloTop::eval_commit() {
    u32 head = rob_rp_.head.read(), last = rob_rp_.last.read();
    u32 lsq_hd = lsq_rp_.head.read(), lsq_lt = lsq_rp_.last.read();
    u8 flc = 0;

    while (head != last && rob_rp_.ready[head].read()) {
        u32 opcode = rob_rp_.opcode[head].read(), rd = rob_rp_.rd[head].read();
        u32 np = rob_rp_.new_pnum[head].read(), op = rob_rp_.old_pnum[head].read();
        u32 lt = rob_rp_.lsq_tag[head].read();
        //fprintf(stderr, "Commit %d %d\n", np, );
        if (rd != 0 && (opcode == 0x33 || opcode == 0x13 || opcode == 0x03 || opcode == 0x6F || opcode == 0x67 || opcode == 0x17 || opcode == 0x37)) {
            if (np != 0) { 
                commit_fl_.push_pregs[flc].write(op); 
                flc++; 
            }
        }

        if (opcode == 0x23) {
            u32 addr = lsq_rp_.addr[lt].read(), data = lsq_rp_.data[lt].read();
            u8 w = static_cast<u8>(lsq_rp_.width[lt].read());
            for (int b = 0; b < w; b++) {
                mem_.buf[addr + b] = (data >> (8 * b)) & 0xFF;
            }
            commit_lsq_.invalidate_req[lt].write(1);
        }
        head = (head + 1) % ROB_SIZE;
    }
    commit_fl_.push_count.write(flc);
    commit_rob_.set_head_valid.write(1); 
    commit_rob_.set_head_val.write(head);

    while (lsq_hd != lsq_lt && !lsq_rp_.valid[lsq_hd].read()) {
        lsq_hd = (lsq_hd + 1) % LSQ_SIZE;
    }
    commit_lsq_.set_head_valid.write(1); 
    commit_lsq_.set_head_val.write(lsq_hd);
}

void TomasuloTop::eval_writeback() {
    int wp = 0;
    for (int i = 0; i < CDB_SIZE; i++) {
        if (!cdb_rp_.valid[i].read()) continue;
        u32 prd = cdb_rp_.prd[i].read(), res = cdb_rp_.result[i].read(), rbt = cdb_rp_.rob_tag[i].read();
        if (prd != 0) {
            //if (prd == 123) fprintf(stderr, "%d\n", cdb_rp_.rob_tag)
            wb_prf_.valid[wp].write(1);
            wb_prf_.preg[wp].write(prd);
            wb_prf_.data[wp].write(res);
            wp++;
            wb_ready_.set_req[prd].write(1);
        }
        if (rbt != NONE_ROB_TAG) {
            ready_rob_.set_ready_req[rbt].write(1);
        }
        wb_cdb_.clear_req[i].write(1);
    }
}

void TomasuloTop::eval_memory() {
    if (im_.exec_mispredict.read()) return;

    int mp = 0;
    for (int i = 0; i < LSQ_SIZE; i++) {
        if (!lsq_rp_.valid[i].read()) continue;
        if (!lsq_rp_.addr_ready[i].read()) continue;
        if (!lsq_rp_.is_load[i].read()) continue;
        if (lsq_rp_.data_ready[i].read()) continue;

        u32 la = lsq_rp_.addr[i].read(), lh = lsq_rp_.head.read();
        u8  lw = static_cast<u8>(lsq_rp_.width[i].read());
        bool lu = lsq_rp_.is_unsigned[i].read();
        u32 lprd = lsq_rp_.prs2_or_prd[i].read(), lrbt = lsq_rp_.rob_tag[i].read();
        bool addr_safe = true; int cs = -1;

        if (i != static_cast<int>(lh)) {
            int j = (i - 1 + LSQ_SIZE) % LSQ_SIZE;
            while (true) {
                if (lsq_rp_.valid[j].read() && !lsq_rp_.is_load[j].read()) {
                    if (!lsq_rp_.addr_ready[j].read()) { 
                        addr_safe = false; 
                        break; 
                    }
                    if (lsq_rp_.addr[j].read() == la) { 
                        cs = j; 
                        if (!lsq_rp_.data_ready[j].read()) {
                            addr_safe = false; 
                        }
                        break; 
                    }
                }
                if (j == static_cast<int>(lh)) break;
                j = (j - 1 + LSQ_SIZE) % LSQ_SIZE;
            }
        }

        if (!addr_safe) { 
            mem_lsq_.set_mem_wait_req[i].write(1); 
            mem_lsq_.set_mem_wait_val[i].write(0); 
            continue; 
        }

        u32 wait = lsq_rp_.mem_wait[i].read();
        if (wait == 0) {
            wait = MEM_LATENCY; 
        }
        wait--;
        if (wait > 0) { 
            mem_lsq_.set_mem_wait_req[i].write(1); 
            mem_lsq_.set_mem_wait_val[i].write(wait); 
            continue; 
        }

        u32 d = 0;
        if (cs >= 0) {
            d = lsq_rp_.data[cs].read();
        } else { 
            for (int b = 0; b < lw; b++) {
                d |= static_cast<u32>(mem_.buf[la + b]) << (8 * b);
            }
            if (!lu && lw < 4 && (d & (1u << (8 * lw - 1)))) {
                d |= ~((1u << (8 * lw)) - 1); 
            }
        }

        if (lprd != 0) {
            mem_cdb_.push_valid[mp].write(1);
            mem_cdb_.push_prd[mp].write(lprd);
            mem_cdb_.push_result[mp].write(d);
            mem_cdb_.push_rob_tag[mp].write(lrbt);
            mp++;
        }
        else {
            ready_rob_.set_ready_req[lrbt].write(1);
        }
        mem_lsq_.set_load_data_req[i].write(1); 
        mem_lsq_.set_load_data_val[i].write(d);
    }
}

void TomasuloTop::eval_issue() {

    if (im_.issue_halt.read()) return;
    if (im_.exec_mispredict.read()) return;
    if (im_.issue_pred_taken.read()) return;
    u32 raw = im_.fetch_raw_instruction.read();
    if (raw == 0) return;

    //fprintf(stderr, "check\n");

    auto ins = decode(raw);
    u32 pc = im_.fetch_instruction_pc.read();
    if (raw == TERMINATE_INST) {
        im_.issue_halt.write(1);
        return;
    }

    bool wrf = (ins.opcode != 0x23 && ins.opcode != 0x63 && ins.opcode != 0x73 && ins.rd != 0);
    bool mmo = (ins.opcode == 0x3 || ins.opcode == 0x23);

    if (rob_rp_.full.read() || rs_rp_.full.read() || wrf && fl_rp_.empty.read() || mmo && lsq_rp_.full.read()) {
        if (rob_rp_.full.read()) fprintf(stderr, "Rob is full\n");
        if (rs_rp_.full.read()) fprintf(stderr, "rs is full\n");
        if (fl_rp_.empty.read()) fprintf(stderr, "freelist full\n");
        if (lsq_rp_.full.read()) fprintf(stderr, "lsq full\n");
        im_.issue_stall.write(1);
        return;
    }

    // BHT prediction for conditional branches
    if (ins.opcode == 0x63) {
        size_t bht_idx = (pc >> 2) % BHT_SIZE;
        bool pred = (bht_[bht_idx] >= 2);
        bht_pred_[rob_rp_.last.read()] = pred;
        if (pred) {
            im_.issue_pred_taken.write(1);
            im_.issue_pred_target.write(pc + ins.imm);
        }
    }

    // LSQ push
    u32 lsq_tag = 0;
    if (mmo) {
      u32 ppd = (ins.opcode == 0x23) ? rat_rp_.map[ins.rs2].read() : 0;
      issue_lsq_.push_valid.write(1);
      issue_lsq_.push_is_load.write(ins.opcode == 0x3 ? 1 : 0);
      issue_lsq_.push_rob_tag.write(rob_rp_.last.read());
      issue_lsq_.push_prs2_or_prd.write(ppd);
      u8 w = 4;
      if (ins.func3 == 0x0 || ins.func3 == 0x4)
        w = 1;
      else if (ins.func3 == 0x1 || ins.func3 == 0x5)
        w = 2;
      issue_lsq_.push_width.write(w);
      issue_lsq_.push_is_unsigned.write(ins.func3 >= 4 ? 1 : 0);
      lsq_tag = lsq_rp_.last.read();
    }

    u32 prs1 = rat_rp_.map[ins.rs1].read(), prs2 = rat_rp_.map[ins.rs2].read();
    u32 np = 0, op = 0;

    if (wrf) {
        np = fl_rp_.head_val.read();
        issue_fl_.pop_req.write(1);
        op = rat_rp_.map[ins.rd].read();
        issue_rat_.rename_valid.write(1);
        issue_rat_.rename_rd.write(ins.rd);
        issue_rat_.rename_new.write(np);
        issue_ready_.clear_req[np].write(1);
        if (ins.opcode == 0x3) {
            lsq_pnum_.valid.write(1);
            lsq_pnum_.idx.write(static_cast<u8>(lsq_tag));
            lsq_pnum_.val.write(np);
        }
    }

    // RS push
    issue_rs_.push_valid.write(1); issue_rs_.push_opcode.write(ins.opcode);
    issue_rs_.push_ins_func3.write(ins.func3); issue_rs_.push_ins_func7.write(ins.func7);
    issue_rs_.push_prs1.write(prs1); issue_rs_.push_prs2.write(prs2); issue_rs_.push_prd.write(np);
    issue_rs_.push_pc.write(pc); issue_rs_.push_rob_tag.write(rob_rp_.last.read());
    issue_rs_.push_lsq_tag.write(static_cast<u32>(lsq_tag));
    issue_rs_.push_ins_raw.write(ins.raw); issue_rs_.push_ins_imm.write(ins.imm);

    // JAL prediction
    if (ins.opcode == 0x6F) {
        //fprintf(stderr, "[ISS JAL] pc=0x%x rd=%d np=%u op=%u tgt=0x%x\n", pc, ins.rd, np, op, pc + ins.imm);
        im_.issue_pred_taken.write(1); im_.issue_pred_target.write(pc + ins.imm);
    }

    // ROB push
    issue_rob_.push_valid.write(1); issue_rob_.push_opcode.write(ins.opcode);
    issue_rob_.push_rd.write(ins.rd); issue_rob_.push_new.write(np); issue_rob_.push_old.write(op);
    issue_rob_.push_lsq.write(static_cast<u32>(lsq_tag)); issue_rob_.push_ins_raw.write(ins.raw);
}

void TomasuloTop::eval_execute() {
    bool proc[RS_SIZE] = {false}; 
    bool misp = false; 
    int alu = 0;

    while (alu < 2) {
        int oi = -1; 
        size_t od = SIZE_MAX, jt = NONE_ROB_TAG, jd = SIZE_MAX;
        //fprintf(stderr, "traverse\n");
        for (int i = 0; i < RS_SIZE; i++) {
            if (proc[i]) continue;
            if (!rs_rp_.valid[i].read()) {
                proc[i] = true;
                continue;
            }
            u32 dist = (rs_rp_.rob_tag[i].read() - rob_rp_.head.read() + ROB_SIZE) % ROB_SIZE; 
            u32 tmp;
            bool ready1 = fetch_op_wire(cdb_rp_, rt_rp_, prf_rp_, rs_rp_.prs1[i].read(), tmp);
            bool ready2 = fetch_op_wire(cdb_rp_, rt_rp_, prf_rp_, rs_rp_.prs2[i].read(), tmp);
            if (!ready1 || !ready2) {
                if (rs_rp_.opcode[i].read() == 0x67 && dist < jd) {
                    jt = rs_rp_.rob_tag[i].read();
                    jd = dist;
                } 
                //fprintf(stderr, "%0x %d %d\n", rs_rp_.pc[i].read(), ready1, ready2);
                continue;
            }
            if (jt != NONE_ROB_TAG) {
                u32 st = (jt + 1) % ROB_SIZE, ed = rob_rp_.last.read();
                bool nw = (st < ed) ? (rs_rp_.rob_tag[i].read() >= st && rs_rp_.rob_tag[i].read() < ed)
                                : (rs_rp_.rob_tag[i].read() >= st || rs_rp_.rob_tag[i].read() < ed);
                if (nw) continue;
            }
            if (dist < od) {
                oi = i;
                od = dist;
            }
        }
        //fprintf(stderr, "\n");
        if (oi == -1) break;
        proc[oi] = true;

        u32 rs1 = 0, rs2 = 0, opcode = rs_rp_.opcode[oi].read(), f3 = rs_rp_.ins_func3[oi].read(),
            f7 = rs_rp_.ins_func7[oi].read(), pc = rs_rp_.pc[oi].read(), prd = rs_rp_.prd[oi].read(),
            rbt = rs_rp_.rob_tag[oi].read(), lst = rs_rp_.lsq_tag[oi].read(), imm = rs_rp_.ins_imm[oi].read();
        fetch_op_wire(cdb_rp_, rt_rp_, prf_rp_, rs_rp_.prs1[oi].read(), rs1);
        fetch_op_wire(cdb_rp_, rt_rp_, prf_rp_, rs_rp_.prs2[oi].read(), rs2);

        u32 res = 0; 
        bool wcdb = true;
        switch (opcode) {
            case 0x33: 
                res = ALU_R(f3, f7, rs1, rs2);
                break;
            case 0x13:
                res = ALU_I(f3, f7, rs1, imm);
                break;
            case 0x37:
                res = imm;
                break;
            case 0x17:
                res = pc + imm;
                break;
            case 0x03:
                res = rs1 + imm;
                exec_lsq_.set_addr_ready_req[lst].write(1);
                exec_lsq_.set_addr_val[lst].write(res);
                wcdb = false;
                break;
            case 0x23:
                res = rs1 + imm;
                exec_lsq_.set_addr_ready_req[lst].write(1);
                exec_lsq_.set_addr_val[lst].write(res);
                exec_lsq_.set_store_data_req[lst].write(1);
                exec_lsq_.set_store_data_val[lst].write([&]() {
                switch (f3) {
                    case 0x0:
                        return rs2 & 0xFF;
                    case 0x1:
                        return rs2 & 0xFFFF;
                    default:
                       return rs2;
                }
              }());
              ready_rob_.set_ready_req[rbt].write(1);
              wcdb = false;
              break;
            case 0x63: {
                branch_count_++;
                bool taken = branch_cond(f3, rs1, rs2);
                size_t bht_idx = (pc >> 2) % BHT_SIZE;
                bool pred = bht_pred_[rbt];

                if (taken != pred) {
                    mispredict_count_++;
                    im_.exec_mispredict.write(1);
                    im_.exec_correct_pc.write(taken ? pc + imm : pc + 4);
                    flush_pipeline(rbt);
                    misp = true;
                }
                // update 2-bit saturating counter
                if (taken) bht_[bht_idx] = (bht_[bht_idx] < 3) ? static_cast<uint8_t>(bht_[bht_idx] + 1) : static_cast<uint8_t>(3);
                else       bht_[bht_idx] = (bht_[bht_idx] > 0) ? static_cast<uint8_t>(bht_[bht_idx] - 1) : static_cast<uint8_t>(0);

                ready_rob_.set_ready_req[rbt].write(1);
                wcdb = false;
                break;
            }
            case 0x6F:
                res = pc + 4;
                //fprintf(stderr, "[JAL]  pc=0x%x prd=%u res=0x%x\n", pc, prd, res);
                break;
            case 0x67: {
                u32 t = (rs1 + imm) & ~1u;
                res = pc + 4;
                u32 pr1 = rs_rp_.prs1[oi].read();
                //fprintf(stderr, "[JALR] pc=0x%x prs1=%u rs1=0x%x rdy=%d imm=0x%x target=0x%x CDB:", pc, pr1, rs1, rt_rp_.ready[pr1].read(), imm, t);
                im_.exec_mispredict.write(1);
                im_.exec_correct_pc.write(t);
                flush_pipeline(rbt);
                misp = true;
                break;
            }
            case 0x0F:
                res = 0;
                wcdb = false;
                break;
            default: 
                fprintf(stderr, "unknown op:0x%x pc=0x%x\n", opcode, pc);
                exit(1);
        }
        if (wcdb) {
            int p = 0;
            while (exec_cdb_.push_valid[p].read()) {
                p++;
            }
            exec_cdb_.push_valid[p].write(1);
            exec_cdb_.push_prd[p].write(prd);
            exec_cdb_.push_result[p].write(res);
            exec_cdb_.push_rob_tag[p].write(rbt);
        }
        exec_rs_.clear_idx[alu].write(static_cast<u8>(oi));
        exec_rs_.clear_count.write(static_cast<u8>(alu + 1));
        alu++; 
        if (misp) {
            break;
        }
    }
}

void TomasuloTop::eval_fetch() {
    // fprintf(stderr, "halt: %d stall %d mispredict %d pred_taken %d\n", im_.issue_halt.read(), im_.issue_stall.read(), im_.exec_mispredict.read(), im_.issue_pred_taken.read());
    if (im_.issue_halt.read()) return;
    if (im_.issue_stall.read()) return;

    u32 pc = im_.fetch_pc.read(), addr;
    if (im_.exec_mispredict.read()) {
      addr = im_.exec_correct_pc.read();
    } else if (im_.issue_pred_taken.read()) {
      addr = im_.issue_pred_target.read();
    } else {
      addr = pc;
    }
    u32 raw = std::bit_cast<u32>(std::array<u8, 4>{mem_.buf[addr], mem_.buf[addr + 1], mem_.buf[addr + 2], mem_.buf[addr + 3]});
    im_.fetch_instruction_pc.write(addr);
    im_.fetch_raw_instruction.write(raw);
    //fprintf(stderr, "new pc = %0x\n", addr + 4);
    im_.fetch_pc.write(addr + 4);
    im_.exec_mispredict.write(0);
    im_.issue_pred_taken.write(0);
}

void TomasuloTop::tick() {
    // Clear all write Wires to 0 (Wires persist values across cycles)
    wb_prf_.clear(); wb_ready_.clear(); 
    issue_ready_.clear(); issue_rat_.clear(); issue_fl_.clear(); issue_rob_.clear(); 
    issue_rs_.clear(); issue_lsq_.clear(); lsq_pnum_.clear(); 
    flush_ready_.clear(); flush_rat_.clear(); flush_fl_.clear(); flush_rob_.clear(); 
    commit_fl_.clear(); commit_rob_.clear(); commit_lsq_.clear();
    ready_rob_.clear();
    exec_rs_.clear(); exec_lsq_.clear(); exec_cdb_.clear(); 
    mem_lsq_.clear(); mem_cdb_.clear(); 
    wb_cdb_.clear();
    // Clear inter-module control Wires each cycle
    im_.issue_stall.write(0);

    // Step 0: storage modules drive read port wires
    prf_.drive_read_ports(prf_rp_); ready_table_.drive_read_ports(rt_rp_);
    rat_.drive_read_ports(rat_rp_); free_list_.drive_read_ports(fl_rp_);
    cdb_.drive_read_ports(cdb_rp_); rs_.drive_read_ports(rs_rp_);
    rob_.drive_read_ports(rob_rp_); lsq_.drive_read_ports(lsq_rp_);

    //fprintf(stderr, "%d %0x\n", rat_rp_.map[5].read(), prf_rp_.data[rat_rp_.map[5].read()].read());

    // Step 1: processing modules eval (read wires → write per-writer wires)
    eval_fetch();eval_commit(); eval_issue(); eval_memory();
    eval_execute(); eval_writeback();

    // Step 2: storage modules eval (per-writer wires → internal Register.next)
    prf_.eval(wb_prf_);
    ready_table_.eval(wb_ready_, issue_ready_, flush_ready_);
    rat_.eval(issue_rat_, flush_rat_);
    free_list_.eval(issue_fl_, commit_fl_, flush_fl_);
    rob_.eval(issue_rob_, commit_rob_, flush_rob_, ready_rob_);
    rs_.eval(issue_rs_, exec_rs_);
    lsq_.eval(issue_lsq_, lsq_pnum_, exec_lsq_, mem_lsq_, commit_lsq_);
    cdb_.eval(exec_cdb_, mem_cdb_, wb_cdb_);

    // x0 force
    prf_.force(0, 0); ready_table_.force(0, true);

    // Step 3: tick all
    prf_.tick(); ready_table_.tick(); rat_.tick(); free_list_.tick();
    cdb_.tick(); rs_.tick(); rob_.tick(); lsq_.tick();

}

void TomasuloTop::run() {
    int ret = 0;
    im_.fetch_pc.write(0);
    im_.exec_mispredict.write(0);
    im_.issue_pred_taken.write(0);
    while (true) {
        if (im_.issue_halt.read() && rob_rp_.empty.read()) {
            u32 pr10 = rat_rp_.map[10].read();
            ret = prf_rp_.data[pr10].read() & 0xFF; 
            break;
        }
        clock_++;
        // fprintf(stderr, "clock = %zu pc = %0x\n", clock_, im_.fetch_pc.read());
        //if (clock_ % 100000 == 0) fprintf(stderr, "clock = %zu pc = %0x\n", clock_, im_.fetch_pc.read());
        tick();
    }
    fprintf(stdout, "%d\n", ret);
    fprintf(stderr, "clock=%zu %zu / %zu = %0.2f\n", clock_, branch_count_ - mispredict_count_, branch_count_, static_cast<double>(branch_count_ - mispredict_count_) / branch_count_);
}
