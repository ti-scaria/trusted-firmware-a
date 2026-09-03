/*
 * Copyright (c) 2024-2026, Texas Instruments Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef K3_LPM_CTRL_H
#define K3_LPM_CTRL_H

#include <plat/common/platform.h>

/*
 * Copy the A53 LPM stub from the BL31 binary to WKUP SRAM.
 *
 * Must be called before the first suspend attempt. Runs from DDR.
 * Return 0 on success, negative error code otherwise.
 */
int32_t k3low_lpm_stub_copy_to_sram(void);

/*
 * Enable or disable low power mode wake-up sources.
 *
 * enable: true to enable all sources, false to disable.
 */
void k3low_config_wake_sources(bool enable);

/*
 * Program the resume magic words in WKUP CTRL MMR.
 *
 * mode: LPM mode to enter.
 */
void k3low_lpm_config_magic_words(uint32_t mode);

/*
 * Set or remove IO isolation.
 *
 * enable: true to isolate IOs, false to release isolation.
 * Return 0 on success, negative error code otherwise.
 */
int32_t k3low_lpm_set_io_isolation(bool enable);

/*
 * Disable the MMU and jump to the WKUP SRAM stub.
 *
 * mode: LPM mode to enter.
 */
void k3low_suspend_to_ram(uint32_t mode);

#endif /* K3_LPM_CTRL_H */
