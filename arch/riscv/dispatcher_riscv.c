/*
  This file is part of MAMBO, a low-overhead dynamic binary modification tool:
      https://github.com/beehive-lab/mambo

  Copyright 2020 Guillermo Callaghan <guillermocallaghan at hotmail dot com>
  Copyright 2020 The University of Manchester

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <assert.h>

#include "dbm.h"
#include "scanner_common.h"

/*
      Algorithm for linking:
        * look up both the target and the fallthrough
        * if lookup(target) fits within +-4KiB
          * BRANCH lookup(target)
          * if lookup(fallthrough) JA/{PUSH + JALR} lookup(fallthrough)
          * return
        * else if lookup(fallthrough) fits within +-4KiB
          * BRANCH(inv) lookup(target)
          * if lookup(fallthrough) JA/{PUSH + JALR} lookup(target)
          * return
        ---
        [not handled at the moment]
        * else if lookup(target) > +-1MiB && lookup(fallthrough) && lookup(fallthrough) > +- 1MiB
          * PUSH {a0, a1}
          * BRANCH invert(cond)
          * JALR lookup(target)
          * JALR lookup(fallthrough)
          * return
        ---
        * else -
          * BRANCH invert(cond)
          * JA/{PUSH + JALR} lookup(target)
          * if lookup(fallthrough) JA/{PUSH + JALR} lookup(fallthrough)

       Target/fallthrough linked, within +/-4KiB
         BRANCH translation target+12
         skip:

       Target linked, within +/-1MB
         BRANCH skip
         JAL zero, translation target+12
         skip:

       Target linked, range greater than +/-1MB
         BRANCH skip                                       4b
         ADDI sp, sp, -16                                  2/4b
         SD a0, 0(sp)                                      2/4b
         SD a1, 8(sp)                                      2/4b
         AUIPC a0, (target offset + 0x800) >> 12           4b
         JALR zero, a0, translation target offset from a0  4b
         skip:                                             18b(c) / 24b

      Both paths linked, both greater than +/-1MB
         ADDI sp, sp, -16
         SD a0, 0(sp)
         SD a1, 8(sp)
         BRANCH skip
         AUIPC a0, (target offset + 0x800) >> 12
         JALR zero, a0, translation target offset from a0
         skip:
         AUIPC a0, (fallthrough offset + 0x800) >> 12
         JALR zero, a0, translation fallthrough offset from a0
*/

<<<<<<< HEAD
int riscv_link_to(uint16_t **o_write_p, uintptr_t target) {
  int ret = riscv_jal_helper(o_write_p, target+6, zero);
  if (ret != 0) {
    riscv_push(o_write_p, (1 << a0) | (1 << a1));
    ret = riscv_jalr_helper(o_write_p, target, zero, a0);
=======
intptr_t increase_or_decrease_addr(intptr_t offset) {
  if (offset > 0) {
    return ((1024 * 1024) - (256*6));
  } else {
    return -((1024 * 1024) - (256*6));
  }
}

int find_basic_block(dbm_thread *thread_data, uintptr_t target, intptr_t offset) {
  int current_bb = 0;
  //refactor!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  if (offset > 0) {
    current_bb = addr_to_bb_id(thread_data, target - increase_or_decrease_addr(offset));
  } else {
    current_bb = addr_to_bb_id(thread_data, target - increase_or_decrease_addr(offset));
  }
  for (int i = 0; i < 6; i++) {
    if (thread_data->code_cache_meta[current_bb].jump_trampoline_availability & 0xFF != 0) {
      return current_bb;
    } else {
      if (offset > 0) {
        current_bb++;
      } else {
        current_bb--;
      }
    }
  }
  return -1;
}

int first_free_trampoline_spot(dbm_thread *thread_data, int bb) {
  int free_spot = 0;
  uint8_t value = thread_data->code_cache_meta[bb].jump_trampoline_availability;
  for (int i = 0; i < 8; i++) {
    if ((value >> 0) & 0x1) {
      free_spot = i;
      break;
    }
  }
  return free_spot;
}

int riscv_link_to(dbm_thread *thread_data, uint16_t **o_write_p, uintptr_t target) {
  uint16_t *write_p = *o_write_p;
  intptr_t offset = target - (intptr_t)*o_write_p;
  int ret = riscv_jal_helper(&write_p, target+6, zero);
   if (ret != 0) {
    if (abs(offset) < ((1024 * 1024 * 2) - 1024 * 6) && target < thread_data->code_cache->traces) {
      intptr_t remaining_offset = offset;
      uintptr_t current_target = target + 6;
      int current_bb = 0;
      while (abs(current_target - (intptr_t)write_p) > (1024 * 1024) - (256 * 12)) {
        current_bb = find_basic_block(thread_data, current_target, offset);
        if (current_bb == -1) {
          ret = -1;
          break;
        } else {
          uint16_t *trampoline_start = thread_data->code_cache_meta[current_bb].jump_trampoline_start;
          int free_spot = first_free_trampoline_spot(thread_data, current_bb);
          trampoline_start += 2*free_spot;
          if (abs(current_target - (intptr_t)trampoline_start) > (1024*1024)) {
            fprintf(stderr, "OH NOOOOOOOoO %llu\n", abs(current_target - (intptr_t)trampoline_start));
            while(1);
          }
          assert(riscv_jal_helper(&trampoline_start, current_target, zero) == 0);
          current_target = trampoline_start-2;
          thread_data->code_cache_meta[current_bb].jump_trampoline_availability ^= (1 << free_spot);
          remaining_offset -= (target - current_target);
          ret = 1;
        }
      }
      
      if (ret != -1) {
         if (abs(current_target-(intptr_t)write_p) > (1024*1024)) {
            fprintf(stderr, "OHhhhh %lld, %lld\n", current_target-(intptr_t)write_p, remaining_offset);
            while(1);
          }
      assert(riscv_jal_helper(&write_p, current_target, zero) == 0);

#ifdef DBM_TRACES
    for (int i = 0; i < 4; i++) {
      riscv_addi(&write_p, zero, zero, 0); // NOP
      write_p += 2;
    }
#endif
    ret = 0;
      }

    }
    if (ret != 0) {
      riscv_push(&write_p, (1 << a0) | (1 << a1));
      ret = riscv_jalr_helper(&write_p, target, zero, a0);
    }
  } else {
#ifdef DBM_TRACES
    for (int i = 0; i < 4; i++) {
      riscv_addi(&write_p, zero, zero, 0); // NOP
      write_p += 2;
    }
#endif
>>>>>>> c2aa790... Jump Trampolining - VERY EARLY VERSION
  }
  return ret;
}

void riscv_link_branch(dbm_thread *thread_data, int bb_id, uintptr_t target) {
  dbm_code_cache_meta *bb_meta = &thread_data->code_cache_meta[bb_id];
  uint16_t *write_p = bb_meta->exit_branch_addr;

  uintptr_t target_tpc = cc_lookup(thread_data, bb_meta->branch_taken_addr);
  int target_in_cc = target_tpc != UINT_MAX;
  uintptr_t fallthrough_tpc = cc_lookup(thread_data, bb_meta->branch_skipped_addr);
  int fallthrough_in_cc = fallthrough_tpc != UINT_MAX;
  assert(target_in_cc || fallthrough_in_cc);

  if (target_in_cc &&
      riscv_branch_helper(&write_p, target_tpc+6, bb_meta->rs1, bb_meta->rs2,
      bb_meta->branch_condition) == 0) {
    if (fallthrough_in_cc) {
      int ret = riscv_link_to(&write_p, fallthrough_tpc);
      assert(ret == 0);
    }
  } else if (fallthrough_in_cc &&
      riscv_branch_helper(&write_p, target_tpc+6, bb_meta->rs1, bb_meta->rs2,
      invert_cond(bb_meta->branch_condition)) == 0) {
    if (target_in_cc) {
      int ret = riscv_link_to(&write_p, target_tpc);
      assert(ret == 0);
    }
  } else {
    uint16_t *branch = write_p;
    write_p += 2;
    enum branch_condition cond = invert_cond(bb_meta->branch_condition);
    if (!target_in_cc) {
      target_tpc = fallthrough_tpc;
      cond = invert_cond(cond);
      fallthrough_in_cc = 0;
    }
    int ret = riscv_link_to(&write_p, target_tpc);
    assert(ret == 0);
    ret = riscv_branch_helper(&branch, (uintptr_t)write_p, bb_meta->rs1, bb_meta->rs2, cond);
    assert(ret == 0);

    if (fallthrough_in_cc) {
      int ret = riscv_link_to(&write_p, fallthrough_tpc);
      assert(ret == 0);
    }
  }
  __clear_cache((void *)bb_meta->exit_branch_addr, (void *)write_p);
}

void dispatcher_riscv(dbm_thread *thread_data, uint32_t source_index, branch_type exit_type,
                      uintptr_t target, uintptr_t block_address) {
  switch (exit_type) {
#ifdef DBM_LINK_COND_IMM
    case branch_riscv:
      riscv_link_branch(thread_data, source_index, target);
      break;
#endif
#ifdef DBM_LINK_UNCOND_IMM
  #warning DBM_LINK_UNCOND_IMM not implemented for RISCV
    case jal_riscv:
      break;
#endif
  }
}
