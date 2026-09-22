/*
 * Copyright (C) 2024-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>

#include <lib/mmio.h>
#include <plat/common/platform.h>

#include <lpm_pll_16fft_raw.h>
#include <lpm_timeout.h>
#include <lpm_trace.h>


/* 16FFT PLL Registers */
#define PLL_16FFT_CFG_OFFSET			(0x08U)

#define PLL_16FFT_LOCKKEY0_OFFSET		(0x10U)
#define PLL_16FFT_LOCKKEY0_VALUE		(0x68EF3490U)

#define PLL_16FFT_LOCKKEY1_OFFSET		(0x14U)
#define PLL_16FFT_LOCKKEY1_VALUE		(0xD172BC5AU)

#define PLL_16FFT_CTRL_OFFSET			(0x20U)
#define PLL_16FFT_CTRL_BYPASS_EN		BIT(31)
#define PLL_16FFT_CTRL_BYP_ON_LOCKLOSS		BIT(16)
#define PLL_16FFT_CTRL_PLL_EN			BIT(15)
#define PLL_16FFT_CTRL_INTL_BYP_EN		BIT(8)
#define PLL_16FFT_CTRL_CLK_4PH_EN		BIT(5)
#define PLL_16FFT_CTRL_CLK_POSTDIV_EN		BIT(4)
#define PLL_16FFT_CTRL_DSM_EN			BIT(1)
#define PLL_16FFT_CTRL_DAC_EN			BIT(0)

#define PLL_16FFT_STAT_OFFSET			(0x24U)
#define PLL_16FFT_STAT_LOCK			BIT(0)

#define PLL_16FFT_FREQ_CTRL0_OFFSET		(0x30U)

#define PLL_16FFT_FREQ_CTRL1_OFFSET		(0x34U)

#define PLL_16FFT_DIV_CTRL_OFFSET		(0x38U)

#define PLL_16FFT_CAL_CTRL_OFFSET		(0x60U)
#define PLL_16FFT_CAL_CTRL_CAL_EN		BIT(31)
#define PLL_16FFT_CAL_CTRL_FAST_CAL		BIT(20)
#define PLL_16FFT_CAL_CTRL_CAL_CNT_SHIFT	(16U)
#define PLL_16FFT_CAL_CTRL_CAL_CNT_MASK		GENMASK(18, 16)
#define PLL_16FFT_CAL_CTRL_CAL_BYP		BIT(15)
#define PLL_16FFT_CAL_CTRL_CAL_IN_MASK		GENMASK(11, 0)

#define PLL_16FFT_CAL_STAT_OFFSET		(0x64U)
#define PLL_16FFT_CAL_STAT_CAL_LOCK		BIT(31)

#define PLL_16FFT_HSDIV_CTRL_OFFSET		(0x80U)
#define PLL_16FFT_HSDIV_CTRL_CLKOUT_EN		BIT(15)
#define PLL_16FFT_HSDIV_CTRL_HSDIV_MASK		GENMASK(6, 0)
/*
 * Lock and calibration poll timeouts in 1 us increments.  Chosen to cover
 * worst-case PLL lock time at minimum VCO input frequency (5 MHz).
 */
#define PLL_16FFT_RAW_LOCK_TIMEOUT		(10000U)
#define PLL_16FFT_CAL_LOCK_TIMEOUT		(435000U)
#define PLL_CAL_COUNT_FAST_MODE			(2U)
#define PLL_16FFT_HSDIV_BYPASS_VALUE		(0x8001U)

/* Configure PLL calibration option 3 (fast calibration mode). */
static void pll_cal_option3(uintptr_t pll_base)
{
	uint32_t cal;

	cal = mmio_read_32(pll_base + PLL_16FFT_CAL_CTRL_OFFSET);

	/* Enable fast cal mode */
	cal |= PLL_16FFT_CAL_CTRL_FAST_CAL;

	/* Disable calibration bypass */
	cal &= ~PLL_16FFT_CAL_CTRL_CAL_BYP;

	/* Set CALCNT to 2 */
	cal &= ~PLL_16FFT_CAL_CTRL_CAL_CNT_MASK;
	cal |= PLL_CAL_COUNT_FAST_MODE << PLL_16FFT_CAL_CTRL_CAL_CNT_SHIFT;

	/* Set CAL_IN to 0 */
	cal &= ~PLL_16FFT_CAL_CTRL_CAL_IN_MASK;

	/* Note this register does not readback the written value. */
	mmio_write_32(pll_base + PLL_16FFT_CAL_CTRL_OFFSET, cal);

	/* Wait 1us before enabling the CAL_EN field */
	k3low_lpm_delay_1us();

	cal = mmio_read_32(pll_base + PLL_16FFT_CAL_CTRL_OFFSET);

	/* Enable calibration for FRACF */
	cal |= PLL_16FFT_CAL_CTRL_CAL_EN;

	/* Note this register does not readback the written value. */
	mmio_write_32(pll_base + PLL_16FFT_CAL_CTRL_OFFSET, cal);
}

/*
 * Enable or disable the PLL output.
 *
 * pll_base: base address of the PLL.
 * enable:   true to enable, false to disable.
 */
static void pll_enable(uintptr_t pll_base, bool enable)
{
	uint32_t ctrl;

	ctrl = mmio_read_32(pll_base + PLL_16FFT_CTRL_OFFSET);
	if (enable) {
		ctrl |= PLL_16FFT_CTRL_PLL_EN;
	} else {
		ctrl &= ~PLL_16FFT_CTRL_PLL_EN;
	}
	mmio_write_32(pll_base + PLL_16FFT_CTRL_OFFSET, ctrl);
}

/*
 * Enable or disable PLL bypass mode.
 *
 * pll:    pointer to PLL context.
 * enable: true to enable bypass, false to disable.
 */
static void pll_bypass(struct pll_raw_data *pll, bool enable)
{
	uint32_t ctrl;

	ctrl = mmio_read_32(pll->base + PLL_16FFT_CTRL_OFFSET);

	if (enable) {
		ctrl |= PLL_16FFT_CTRL_BYPASS_EN;
	} else {
		ctrl &= ~PLL_16FFT_CTRL_BYPASS_EN;
	}

	mmio_write_32(pll->base + PLL_16FFT_CTRL_OFFSET, ctrl);
}

static void pll_disable_hsdiv(struct pll_raw_data *pll,
					     uint8_t hsdiv)
{
	uint32_t ctrl;

	ctrl = mmio_read_32(pll->base + PLL_16FFT_HSDIV_CTRL_OFFSET +
			    (hsdiv * 0x4U));
	ctrl &= ~PLL_16FFT_HSDIV_CTRL_CLKOUT_EN;

	mmio_write_32(pll->base + PLL_16FFT_HSDIV_CTRL_OFFSET +
		      (hsdiv * 0x4U), ctrl);
}

static void pll_enable_hsdiv(struct pll_raw_data *pll,
					    uint8_t hsdiv)
{
	uint32_t ctrl;

	ctrl = mmio_read_32(pll->base + PLL_16FFT_HSDIV_CTRL_OFFSET +
			    (hsdiv * 0x4U));
	ctrl |= PLL_16FFT_HSDIV_CTRL_CLKOUT_EN;

	mmio_write_32(pll->base + PLL_16FFT_HSDIV_CTRL_OFFSET +
		      (hsdiv * 0x4U), ctrl);
}

int32_t k3low_pll_restore(struct pll_raw_data *pll)
{
	uint8_t i;
	uint32_t ctrl;
	uint32_t cfg;
	uint32_t cal;
	uint32_t timeout;
	bool cal_lock = true;

	/* Unlock write access */
	mmio_write_32(pll->base + PLL_16FFT_LOCKKEY0_OFFSET,
		      PLL_16FFT_LOCKKEY0_VALUE);
	mmio_write_32(pll->base + PLL_16FFT_LOCKKEY1_OFFSET,
		      PLL_16FFT_LOCKKEY1_VALUE);

	/* Make sure that PLL is in bypass mode */
	pll_bypass(pll, true);

	/* Make sure that PLL is disabled */
	pll_enable(pll->base, false);

	/* Restore the divider values */
	mmio_write_32(pll->base + PLL_16FFT_FREQ_CTRL0_OFFSET, pll->freq_ctrl0);
	mmio_write_32(pll->base + PLL_16FFT_FREQ_CTRL1_OFFSET, pll->freq_ctrl1);
	mmio_write_32(pll->base + PLL_16FFT_DIV_CTRL_OFFSET, pll->div_ctrl);

	ctrl = mmio_read_32(pll->base + PLL_16FFT_CTRL_OFFSET);

	/* Always bypass if we lose lock */
	ctrl |= PLL_16FFT_CTRL_BYP_ON_LOCKLOSS;

	/* Prefer glitchless bypass */
	ctrl &= ~PLL_16FFT_CTRL_INTL_BYP_EN;

	/* Always enable output if PLL */
	ctrl |= PLL_16FFT_CTRL_CLK_POSTDIV_EN;

	/* Currently unused by all PLLs */
	ctrl &= ~PLL_16FFT_CTRL_CLK_4PH_EN;

	/* Make sure we have fractional support if required */
	if (pll->freq_ctrl1 != 0U) {
		ctrl |= PLL_16FFT_CTRL_DSM_EN;
		ctrl |= PLL_16FFT_CTRL_DAC_EN;
	} else {
		ctrl &= ~PLL_16FFT_CTRL_DSM_EN;
		ctrl &= ~PLL_16FFT_CTRL_DAC_EN;
	}

	mmio_write_32(pll->base + PLL_16FFT_CTRL_OFFSET, ctrl);

	/* Program all HSDIV outputs */
	cfg = mmio_read_32(pll->base + PLL_16FFT_CFG_OFFSET);
	for (i = 0U; i < 16U; i++) {
		/* Program HSDIV output if present */
		if (((1U << (i + 16U)) & cfg) != 0U) {
			mmio_write_32(pll->base +
				      PLL_16FFT_HSDIV_CTRL_OFFSET +
				      (i * 0x4U), pll->hsdiv[i]);
		}
	}

	if (pll->freq_ctrl1 == 0U) {
		/* Enable Calibration in Integer mode */
		pll_cal_option3(pll->base);
	} else {
		/* Disable Calibration in Fractional mode */
		cal = mmio_read_32(pll->base + PLL_16FFT_CAL_CTRL_OFFSET);
		cal &= ~PLL_16FFT_CAL_CTRL_CAL_EN;
		mmio_write_32(pll->base + PLL_16FFT_CAL_CTRL_OFFSET, cal);
		timeout = PLL_16FFT_CAL_LOCK_TIMEOUT;
		while ((timeout > 0U) &&
		       (((mmio_read_32(pll->base +
				       PLL_16FFT_CAL_STAT_OFFSET) &
			  PLL_16FFT_CAL_STAT_CAL_LOCK) == 0U) == false)) {
			--timeout;
			k3low_lpm_delay_1us();
		}
		if (timeout == 0U) {
			cal_lock = false;
		}
	}

	if (cal_lock == true) {
		/*
		 * Wait at least 1 ref cycle before enabling PLL.
		 * Minimum VCO input frequency is 5MHz, therefore maximum
		 * wait time for 1 ref clock is 0.2us.
		 */
		k3low_lpm_delay_1us();

		/* Make sure PLL is enabled */
		pll_enable(pll->base, true);

		/* Wait for the PLL lock */
		timeout = PLL_16FFT_RAW_LOCK_TIMEOUT;
		while ((timeout > 0U) &&
		       (((mmio_read_32(pll->base + PLL_16FFT_STAT_OFFSET) &
			  PLL_16FFT_STAT_LOCK) == 1U) == false)) {
			--timeout;
			k3low_lpm_delay_1us();
		}
		if (timeout == 0U) {
			cal_lock = false;
		}
	}

	/*
	 * In case of cal lock failure, disable calibration and retry
	 * the PLL lock without it.
	 */
	if (cal_lock == false) {
		/* Disable PLL */
		pll_enable(pll->base, false);

		/* Disable Calibration */
		cal = mmio_read_32(pll->base + PLL_16FFT_CAL_CTRL_OFFSET);
		cal &= ~PLL_16FFT_CAL_CTRL_CAL_EN;
		mmio_write_32(pll->base + PLL_16FFT_CAL_CTRL_OFFSET, cal);
		timeout = PLL_16FFT_CAL_LOCK_TIMEOUT;
		while ((timeout > 0U) &&
		       (((mmio_read_32(pll->base +
				       PLL_16FFT_CAL_STAT_OFFSET) &
			  PLL_16FFT_CAL_STAT_CAL_LOCK) == 0U) == false)) {
			--timeout;
		}
		if (timeout == 0U) {
			return -ETIMEDOUT;
		}

		/* Enable PLL */
		pll_enable(pll->base, true);

		/* Wait for PLL Lock */
		timeout = PLL_16FFT_RAW_LOCK_TIMEOUT;
		while ((timeout > 0U) &&
				(((mmio_read_32(pll->base +
						PLL_16FFT_STAT_OFFSET) &
				PLL_16FFT_STAT_LOCK) == 1U) == false)) {
			--timeout;
		}
		if (timeout == 0U) {
			return -ETIMEDOUT;
		}
	}

	/*
	 * Clear BYPASS_EN to switch clocks to the locked PLL
	 * frequency.
	 */
	pll_bypass(pll, false);
	return 0;
}

void k3low_pll_save(struct pll_raw_data *pll)
{
	uint8_t i;
	uint32_t cfg;

	pll->freq_ctrl0 = mmio_read_32(pll->base + PLL_16FFT_FREQ_CTRL0_OFFSET);
	pll->freq_ctrl1 = mmio_read_32(pll->base + PLL_16FFT_FREQ_CTRL1_OFFSET);
	pll->div_ctrl = mmio_read_32(pll->base + PLL_16FFT_DIV_CTRL_OFFSET);

	/* Read all present HSDIV outputs */
	cfg = mmio_read_32(pll->base + PLL_16FFT_CFG_OFFSET);
	for (i = 0U; i < 16U; i++) {
		if (((1U << (i + 16U)) & cfg) != 0U) {
			pll->hsdiv[i] = mmio_read_32(pll->base +
						     PLL_16FFT_HSDIV_CTRL_OFFSET +
						     (i * 0x4U));
		}
	}
}

void k3low_pll_disable(struct pll_raw_data *pll)
{
	/* Select reference clk for PLL and HSDIV clk outputs */
	pll_bypass(pll, true);
	k3low_pll_bypass_hsdivs(pll);

	/* Disable the PLL */
	pll_enable(pll->base, false);
}

void k3low_pll_bypass_hsdivs(struct pll_raw_data *pll)
{
	uint8_t i;
	uint32_t cfg;

	cfg = mmio_read_32(pll->base + PLL_16FFT_CFG_OFFSET);
	for (i = 0U; i < 16U; i++) {
		if (((1U << (i + 16U)) & cfg) != 0U) {
			mmio_write_32(pll->base +
				      PLL_16FFT_HSDIV_CTRL_OFFSET +
				      (i * 0x4U),
				      PLL_16FFT_HSDIV_BYPASS_VALUE);
		}
	}
}

void k3low_pll_program_hsdiv(struct pll_raw_data *pll,
					    uint8_t hsdiv, uint8_t value)
{
	uint32_t cfg;
	uint32_t ctrl;

	cfg = mmio_read_32(pll->base + PLL_16FFT_CFG_OFFSET);
	/* Program HSDIV output if present */
	if (((1U << (hsdiv + 16U)) & cfg) != 0U) {
		ctrl = mmio_read_32(pll->base +
				    PLL_16FFT_HSDIV_CTRL_OFFSET +
				    (hsdiv * 0x4U));
		/* Clear divider value and set new value */
		ctrl = (ctrl & ~PLL_16FFT_HSDIV_CTRL_HSDIV_MASK) |
		       (value & PLL_16FFT_HSDIV_CTRL_HSDIV_MASK);
		mmio_write_32(pll->base +
			      PLL_16FFT_HSDIV_CTRL_OFFSET +
			      (hsdiv * 0x4U), ctrl);
	}
}

void k3low_pll_disable_hsdivs(struct pll_raw_data *pll,
					     const uint8_t *hsdiv_indices,
					     uint8_t count)
{
	uint8_t i;

	for (i = 0U; i < count; i++) {
		pll_disable_hsdiv(pll, hsdiv_indices[i]);
	}
}

void k3low_pll_enable_hsdivs(struct pll_raw_data *pll,
					    const uint8_t *hsdiv_indices,
					    uint8_t count)
{
	uint8_t i;

	for (i = 0U; i < count; i++) {
		pll_enable_hsdiv(pll, hsdiv_indices[i]);
	}
}
