/*
 * Copyright (C) 2024-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <platform_def.h>

	.globl	k3low_lpm_resume

func k3low_lpm_resume
    mov   x0, #DCCISW
    bl    dcsw_op_all               // Clean & invalidate D-cache

    bl    disable_mmu_icache_el3    // Disable D/I-cache and MMU bit
    ic    iallu                     // Explicitly invalidate I-cache lines
    dsb   sy                        // Ensure invalidation is complete
    isb                             // Flush pipeline

    movz  x0, #(DEVICE_WKUP_SRAM_STACK_BASE_L)
    movk  x0, #(DEVICE_WKUP_SRAM_STACK_BASE_H), lsl #16
    mov   sp, x0
    bl    k3low_lpm_resume_c
    ret
endfunc k3low_lpm_resume
