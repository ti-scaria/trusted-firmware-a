/*
 * Copyright (C) 2024-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef K3_LPM_CTRL_H
#define K3_LPM_CTRL_H

#include <stdint.h>

#include <plat/common/platform.h>

/*
 * Layout descriptor appended as a 16-byte trailer at the end of
 * .lpm_stub_data by lpm_stub.ld.S. All offset fields are byte offsets
 * from DEVICE_WKUP_SRAM_BASE.
 */
struct lpm_stub_header {
	uint32_t resume_offset;		/* offset to k3low_lpm_resume     */
	uint32_t suspend_offset;	/* offset to k3low_lpm_stub_entry */
	uint32_t bss_offset;		/* offset to start of BSS         */
	uint32_t bss_size;		/* size of BSS in bytes           */
};

/*
 * Copy A53 stub from DDR to WKUP SRAM.
 *
 * Return 0 on success, error code otherwise.
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
