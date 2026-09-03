/*
 * Copyright (C) 2014-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LPM_PLL_16FFT_RAW_H
#define LPM_PLL_16FFT_RAW_H

#include <plat/common/platform.h>

struct pll_raw_data {
	uintptr_t	base;
	uint32_t	freq_ctrl0;
	uint32_t	freq_ctrl1;
	uint32_t	div_ctrl;
	uint32_t	hsdiv[16];
};

/*
 * Restore a saved PLL configuration previously captured with k3low_pll_save().
 *
 * pll: pointer to PLL context with base set to the target PLL address.
 * Return 0 on success, negative error code otherwise.
 */
int32_t k3low_pll_restore(struct pll_raw_data *pll);

/*
 * Save a PLL configuration.
 *
 * pll: pointer to PLL context with base set to the PLL address to save.
 */
void k3low_pll_save(struct pll_raw_data *pll);

/*
 * Bypass and then disable a PLL.
 *
 * pll: pointer to PLL context with base set to the PLL address.
 */
void k3low_pll_disable(struct pll_raw_data *pll);

/*
 * Unbypass a PLL (switch outputs from reference clock back to PLL output).
 *
 * pll: pointer to PLL context with base set to the PLL address.
 */
void k3low_pll_unbypass(struct pll_raw_data *pll);

/*
 * Put all present HSDIVs of a PLL into bypass mode.
 *
 * pll: pointer to PLL context with base set to the PLL address.
 */
void k3low_pll_bypass_hsdivs(struct pll_raw_data *pll);

/*
 * Program a specific HSDIV divider value.
 *
 * pll:   pointer to PLL context with base set to the PLL address.
 * hsdiv: HSDIV index to program.
 * value: new 7-bit divider value.
 */
void k3low_pll_program_hsdiv(struct pll_raw_data *pll, uint8_t hsdiv,
			     uint8_t value);

/*
 * Disable multiple HSDIVs of a PLL.
 *
 * pll:           pointer to PLL context with base set to the PLL address.
 * hsdiv_indices: array of HSDIV indices to disable.
 * count:         number of entries in hsdiv_indices.
 */
void k3low_pll_disable_hsdivs(struct pll_raw_data *pll,
			      const uint8_t *hsdiv_indices, uint8_t count);

/*
 * Enable multiple HSDIVs of a PLL.
 *
 * pll:           pointer to PLL context with base set to the PLL address.
 * hsdiv_indices: array of HSDIV indices to enable.
 * count:         number of entries in hsdiv_indices.
 */
void k3low_pll_enable_hsdivs(struct pll_raw_data *pll,
			     const uint8_t *hsdiv_indices, uint8_t count);

#endif /* LPM_PLL_16FFT_RAW_H */
