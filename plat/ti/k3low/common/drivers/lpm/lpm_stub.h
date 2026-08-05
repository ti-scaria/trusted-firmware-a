/*
 * Copyright (c) 2024-2026, Texas Instruments Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LPM_STUB_H
#define LPM_STUB_H

#include <plat/common/platform.h>

/*
 * Copy A53 stub code from DDR to WKUP SRAM.
 *
 * Return 0 on success, negative error code otherwise.
 */
int32_t k3low_lpm_stub_copy_to_sram(void);

/* Low power mode resume C-level entry point. */
void k3low_lpm_resume_c(void);

/*
 * Suspend the device into S2R state.
 *
 * mode: LPM mode to enter.
 */
void k3low_suspend_to_ram(uint32_t mode);

/*
 * Enable or disable low power mode wake-up sources.
 *
 * enable: true to enable all sources, false to disable.
 */
void k3low_config_wake_sources(bool enable);

/*
 * WKUP SRAM entry point for the A53 LPM stub.
 *
 * mode: LPM mode to enter.
 */
void k3low_lpm_stub_entry(uint32_t mode);

/*
 * Program the resume magic words in WKUP CTRL MMR.
 *
 * mode: LPM mode to enter.
 */
void k3low_lpm_config_magic_words(uint32_t mode);

/*
 * Check whether the CAN IO magic word is latched.
 *
 * Return true if latched, false otherwise.
 */
bool k3low_lpm_check_can_io_latch(void);

/*
 * Set or remove IO isolation.
 *
 * enable: true to isolate IOs, false to release isolation.
 * Return 0 on success, negative error code otherwise.
 */
int32_t k3low_lpm_set_io_isolation(bool enable);

/* Abort the current LPM sequence with a trace event and spin. */
__wkupsramfunc void k3low_lpm_abort(void);

#endif /* LPM_STUB_H */
