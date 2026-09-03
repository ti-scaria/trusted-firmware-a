/*
 * Copyright (C) 2024-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * LPM stub functions and data that execute from WKUP SRAM.
 */

#include <lib/mmio.h>
#include <plat/common/platform.h>

#include <board_def.h>
#include <lpm_ddr.h>
#include <lpm_pll_16fft_raw.h>
#include <lpm_psc_raw.h>
#include <lpm_stub.h>
#include <lpm_timeout.h>
#include <lpm_trace.h>
#include <ti_sci.h>

#define WFI_STATUS				(0x400U)
#define MPU_TIFS_WFI_MASK			BIT(2)
#define RST_CTRL				(0x4000U)
#define PMCTRL_SYS				(0x80U)
#define WKUP_CTRL_MMR_WKUP_GPIO0_CLKSEL		(0x8000U)
#define WKUP_GPIO0_CLKSEL_CLK_32K		2U
#define PSC_CORE_1_MDSTAT			(0x8A4U)
#define MDSTAT_STATE_MASK			(0x1FU)
#define GP_CORE_CTL				0U
#define PD_DDR					2U
#define LPSC_MAIN_DDR_LOCAL			21U
#define LPSC_MAIN_DDR_CFG_ISO_N			22U
#define LPSC_MAIN_DDR_DATA_ISO_N		23U
#define LPSC_MAIN_GP_USB0			7U
#define LPSC_MAIN_GP_USB0_ISO_N			8U
#define LPSC_MAIN_GP_USB1			9U
#define LPSC_MAIN_GP_USB1_ISO_N			10U

#define TI_MAILBOX_MSG				(0x40U)
#define MAILBOX_MSG_BUFFER_OFFSET		(0x100U)
#define TISCI_MSG_CORE_RESUME			(0x000A0304U)

#define PLLOFFSET(idx)				(0x1000U * (idx))

/* PLL HSDIV0 Configuration for DSS Deep Sleep */
#define PLL_HSDIV0_MAX_DIVIDER_VALUE		(0x0FU)

/* counts of 1us delay for 100ms */
#define TIMEOUT_100MS				100000U

/* Main PLL to be saved and restored */
static struct pll_raw_data main_pll0 = {
	.base = K3_MAIN_PLL_MMR_BASE + PLLOFFSET(0U),
};

static struct pll_raw_data main_pll8 = {
	.base = K3_MAIN_PLL_MMR_BASE + PLLOFFSET(8U),
};

static struct pll_raw_data main_pll17 = {
	.base = K3_MAIN_PLL_MMR_BASE + PLLOFFSET(17U),
};

/* Base addresses of main PLL structures to be saved and restored */
static struct pll_raw_data *main_plls_save_rstr[3] = {
	&main_pll0, &main_pll8, &main_pll17
};

/* HSDIVs to disable on PLL0 in DSS DeepSleep low power mode */
static uint8_t main_pll0_hsdivs_to_disable[] = {2, 3, 4, 5, 6, 7, 9};

static uint32_t num_main_plls_save_rstr = 3U;
static uint8_t usb0_state;
static uint8_t usb1_state;
static uint8_t lpm_mode;

static void config_gpio_clk_mux(uint32_t clk_src)
{
	mmio_write_32(WKUP_CTRL_MMR_SEC_2_BASE +
		      WKUP_CTRL_MMR_WKUP_GPIO0_CLKSEL, clk_src);
}

/* Save main domain PLL configuration before power-down. */
static void save_main_pll(void)
{
	uint32_t i;

	for (i = 0U; i < num_main_plls_save_rstr; i++) {
		k3low_pll_save(main_plls_save_rstr[i]);
	}
}

/* Disable main domain PLLs (or a subset for DSS deep-sleep). */
static void disable_main_pll(uint32_t mode)
{
	uint32_t i;

	if (mode != TI_K3_SLEEP_MODE_DSS_PLUS_DEEP_SLEEP) {
		for (i = 0U; i < num_main_plls_save_rstr; i++) {
			k3low_pll_disable(main_plls_save_rstr[i]);
		}
	} else {
		/* Disable main PLL8 (ARM PLL) */
		k3low_pll_disable(&main_pll8);

		/*
		 * Change HSDIV0 frequency value to lowest possible so that it
		 * is still functional.
		 */
		k3low_pll_program_hsdiv(&main_pll0, 0U,
					PLL_HSDIV0_MAX_DIVIDER_VALUE);

		/* Disable specific HSDIVs on PLL0 (2,3,4,5,6,7,9) - HSDIV8 left untouched */
		k3low_pll_disable_hsdivs(&main_pll0,
					 main_pll0_hsdivs_to_disable,
					 ARRAY_SIZE(main_pll0_hsdivs_to_disable));
	}
}

/*
 * Save current state of USB0 and USB1 LPSCs and their isolation modules,
 * then disable them to allow USB to act as a wake-up source during deep sleep.
 *
 * Return 0 on success, negative error code otherwise.
 */
static int32_t save_and_disable_usb_lpsc(void)
{
	int32_t ret = 0;

	usb0_state = k3low_psc_raw_lpsc_get_state(K3_MAIN_PSC_BASE,
						   LPSC_MAIN_GP_USB0);
	k3low_psc_raw_lpsc_set_state(K3_MAIN_PSC_BASE, LPSC_MAIN_GP_USB0,
				     MDCTL_STATE_DISABLE, false);
	k3low_psc_raw_pd_initiate(K3_MAIN_PSC_BASE, GP_CORE_CTL);
	ret = k3low_psc_raw_pd_wait(K3_MAIN_PSC_BASE, GP_CORE_CTL);

	if (ret == 0) {
		k3low_psc_raw_lpsc_set_state(K3_MAIN_PSC_BASE,
					     LPSC_MAIN_GP_USB0_ISO_N,
					     MDCTL_STATE_DISABLE, false);
		k3low_psc_raw_pd_initiate(K3_MAIN_PSC_BASE, GP_CORE_CTL);
		ret = k3low_psc_raw_pd_wait(K3_MAIN_PSC_BASE, GP_CORE_CTL);
	}

	if (ret == 0) {
		usb1_state = k3low_psc_raw_lpsc_get_state(K3_MAIN_PSC_BASE,
							   LPSC_MAIN_GP_USB1);
		k3low_psc_raw_lpsc_set_state(K3_MAIN_PSC_BASE,
					     LPSC_MAIN_GP_USB1,
					     MDCTL_STATE_DISABLE, false);
		k3low_psc_raw_pd_initiate(K3_MAIN_PSC_BASE, GP_CORE_CTL);
		ret = k3low_psc_raw_pd_wait(K3_MAIN_PSC_BASE, GP_CORE_CTL);
	}

	if (ret == 0) {
		k3low_psc_raw_lpsc_set_state(K3_MAIN_PSC_BASE,
					     LPSC_MAIN_GP_USB1_ISO_N,
					     MDCTL_STATE_DISABLE, false);
		k3low_psc_raw_pd_initiate(K3_MAIN_PSC_BASE, GP_CORE_CTL);
		ret = k3low_psc_raw_pd_wait(K3_MAIN_PSC_BASE, GP_CORE_CTL);
	}

	return ret;
}

/*
 * Restore USB0 and USB1 LPSCs and their isolation modules to the states
 * saved during suspend.
 *
 * Return 0 on success, negative error code otherwise.
 */
static int32_t restore_usb_lpsc(void)
{
	int32_t ret;

	k3low_psc_raw_lpsc_set_state(K3_MAIN_PSC_BASE, LPSC_MAIN_GP_USB0,
				     usb0_state, false);
	k3low_psc_raw_pd_initiate(K3_MAIN_PSC_BASE, GP_CORE_CTL);
	ret = k3low_psc_raw_pd_wait(K3_MAIN_PSC_BASE, GP_CORE_CTL);

	if (ret == 0) {
		k3low_psc_raw_lpsc_set_state(K3_MAIN_PSC_BASE,
					     LPSC_MAIN_GP_USB0_ISO_N,
					     usb0_state, false);
		k3low_psc_raw_pd_initiate(K3_MAIN_PSC_BASE, GP_CORE_CTL);
		ret = k3low_psc_raw_pd_wait(K3_MAIN_PSC_BASE, GP_CORE_CTL);
	}

	if (ret == 0) {
		k3low_psc_raw_lpsc_set_state(K3_MAIN_PSC_BASE,
					     LPSC_MAIN_GP_USB1,
					     usb1_state, false);
		k3low_psc_raw_pd_initiate(K3_MAIN_PSC_BASE, GP_CORE_CTL);
		ret = k3low_psc_raw_pd_wait(K3_MAIN_PSC_BASE, GP_CORE_CTL);
	}

	if (ret == 0) {
		k3low_psc_raw_lpsc_set_state(K3_MAIN_PSC_BASE,
					     LPSC_MAIN_GP_USB1_ISO_N,
					     usb1_state, false);
		k3low_psc_raw_pd_initiate(K3_MAIN_PSC_BASE, GP_CORE_CTL);
		ret = k3low_psc_raw_pd_wait(K3_MAIN_PSC_BASE, GP_CORE_CTL);
	}

	return ret;
}

/* Disable DDR LPSC and power domain. */
static int32_t disable_ddr_lpsc(void)
{
	int32_t ret;

	k3low_psc_raw_lpsc_set_state(K3_MAIN_PSC_BASE,
				     LPSC_MAIN_DDR_DATA_ISO_N,
				     MDCTL_STATE_SWRSTDISABLE, false);
	k3low_psc_raw_pd_initiate(K3_MAIN_PSC_BASE, PD_DDR);
	ret = k3low_psc_raw_pd_wait(K3_MAIN_PSC_BASE, PD_DDR);

	if (ret == 0) {
		k3low_psc_raw_lpsc_set_state(K3_MAIN_PSC_BASE,
					     LPSC_MAIN_DDR_CFG_ISO_N,
					     MDCTL_STATE_SWRSTDISABLE, false);
		k3low_psc_raw_pd_initiate(K3_MAIN_PSC_BASE, PD_DDR);
		ret = k3low_psc_raw_pd_wait(K3_MAIN_PSC_BASE, PD_DDR);
	}

	if (ret == 0) {
		k3low_psc_raw_lpsc_set_state(K3_MAIN_PSC_BASE,
					     LPSC_MAIN_DDR_LOCAL,
					     MDCTL_STATE_SWRSTDISABLE, false);
		k3low_psc_raw_pd_initiate(K3_MAIN_PSC_BASE, PD_DDR);
		ret = k3low_psc_raw_pd_wait(K3_MAIN_PSC_BASE, PD_DDR);
	}

	return ret;
}

/* Enable DDR LPSC and power domain. */
static int32_t enable_ddr_lpsc(void)
{
	int32_t ret;

	k3low_psc_raw_lpsc_set_state(K3_MAIN_PSC_BASE, LPSC_MAIN_DDR_LOCAL,
				     MDCTL_STATE_ENABLE, false);
	k3low_psc_raw_pd_initiate(K3_MAIN_PSC_BASE, PD_DDR);
	ret = k3low_psc_raw_pd_wait(K3_MAIN_PSC_BASE, PD_DDR);

	if (ret == 0) {
		k3low_psc_raw_lpsc_set_state(K3_MAIN_PSC_BASE,
					     LPSC_MAIN_DDR_CFG_ISO_N,
					     MDCTL_STATE_ENABLE, false);
		k3low_psc_raw_pd_initiate(K3_MAIN_PSC_BASE, PD_DDR);
		ret = k3low_psc_raw_pd_wait(K3_MAIN_PSC_BASE, PD_DDR);
	}

	if (ret == 0) {
		k3low_psc_raw_lpsc_set_state(K3_MAIN_PSC_BASE,
					     LPSC_MAIN_DDR_DATA_ISO_N,
					     MDCTL_STATE_ENABLE, false);
		k3low_psc_raw_pd_initiate(K3_MAIN_PSC_BASE, PD_DDR);
		ret = k3low_psc_raw_pd_wait(K3_MAIN_PSC_BASE, PD_DDR);
	}

	return ret;
}

/* Restore main domain PLLs after power-up. */
static int32_t restore_main_pll(void)
{
	uint32_t i;
	int32_t ret;

	if (lpm_mode == TI_K3_SLEEP_MODE_DSS_PLUS_DEEP_SLEEP) {
		ret = k3low_pll_restore(&main_pll8);
		if (ret != 0) {
			return ret;
		}

		/* Restore HSDIV0 frequency value to original value */
		k3low_pll_program_hsdiv(&main_pll0, 0U, main_pll0.hsdiv[0]);

		/* Enable specific HSDIVs on PLL0 (2,3,4,5,6,7,9) */
		k3low_pll_enable_hsdivs(&main_pll0,
					main_pll0_hsdivs_to_disable,
					ARRAY_SIZE(main_pll0_hsdivs_to_disable));
	} else {
		for (i = 0U; i < num_main_plls_save_rstr; i++) {
			ret = k3low_pll_restore(main_plls_save_rstr[i]);
			if (ret != 0) {
				return ret;
			}
		}
	}

	return 0;
}

void k3low_lpm_abort(void)
{
	volatile int32_t a = 0x1234;

	lpm_seq_trace_fail(LPM_SEQ_ABORT);
	while (a != 0) {
		wfi();
	}
}

/* Poll TIFS WFI status; return true when TIFS is in WFI, false on timeout. */
static bool lpm_sleep_wait_for_tifs_wfi(void)
{
	uint32_t reg;
	int32_t i = (int32_t)TIMEOUT_100MS;

	do {
		reg = mmio_read_32(WKUP_CTRL_MMR_SEC_5_BASE + WFI_STATUS);
		if ((reg & MPU_TIFS_WFI_MASK) == MPU_TIFS_WFI_MASK) {
			return true;
		}
		i--;
		k3low_lpm_delay_1us();
	} while (i != 0);
	return false;
}

/* Poll secondary core MDSTAT; return true when powered off, false on timeout. */
static bool lpm_wait_for_secondary_core_down(void)
{
	uint32_t reg;
	int32_t i = (int32_t)TIMEOUT_100MS;

	do {
		reg = mmio_read_32(K3_MAIN_PSC_BASE + PSC_CORE_1_MDSTAT);
		if ((reg & MDSTAT_STATE_MASK) == MDCTL_STATE_SWRSTDISABLE) {
			return true;
		}
		i--;
		k3low_lpm_delay_1us();
	} while (i != 0);
	return false;
}

__section(".wkupsram.suspend_entry") void k3low_lpm_stub_entry(uint32_t mode)
{
	lpm_mode = (uint8_t)mode;

	if (mode == TI_K3_SLEEP_MODE_RTC_PLUS_DDR) {
		/* Wait for a53_1 to turn off */
		if (lpm_wait_for_secondary_core_down() == false) {
			lpm_seq_trace_fail(LPM_SEQ_SECONDARY_CORE_DOWN);
			k3low_lpm_abort();
		} else {
			lpm_seq_trace(LPM_SEQ_SECONDARY_CORE_DOWN);
		}

		if (lpm_sleep_wait_for_tifs_wfi() == false) {
			lpm_seq_trace_fail(LPM_SEQ_TIFS_WFI_WAIT);
			k3low_lpm_abort();
		} else {
			lpm_seq_trace(LPM_SEQ_TIFS_WFI_WAIT);
		}

		/* Place DDR into self-refresh */
		if (k3low_put_ddr_in_rtc_lpm() != 0) {
			lpm_seq_trace_fail(LPM_SEQ_DDR_SELF_REFRESH);
			k3low_lpm_abort();
		} else {
			lpm_seq_trace(LPM_SEQ_DDR_SELF_REFRESH);
		}

		/* Disable the LPSCs for DDR */
		if (disable_ddr_lpsc() != 0) {
			lpm_seq_trace_fail(LPM_SEQ_DDR_LPSC_DISABLE);
			k3low_lpm_abort();
		} else {
			lpm_seq_trace(LPM_SEQ_DDR_LPSC_DISABLE);
		}

		save_main_pll();
		lpm_seq_trace(LPM_SEQ_SAVE_MAIN_PLL);

		disable_main_pll(mode);
		lpm_seq_trace(LPM_SEQ_DISABLE_MAIN_PLL);

		/* configure the pmic input */
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE + PMCTRL_SYS, 0x0U);
		lpm_seq_trace(LPM_SEQ_PMIC_CONFIG);
		dsb();
		isb();

		for (;;) {
			wfi();
		}

	} else if ((mode == TI_K3_SLEEP_MODE_DEEP_SLEEP) ||
		   (mode == TI_K3_SLEEP_MODE_DSS_PLUS_DEEP_SLEEP)) {

		/* Wait for a53_1 to turn off */
		if (lpm_wait_for_secondary_core_down() == false) {
			lpm_seq_trace_fail(LPM_SEQ_SECONDARY_CORE_DOWN);
			k3low_lpm_abort();
		} else {
			lpm_seq_trace(LPM_SEQ_SECONDARY_CORE_DOWN);
		}
		if (save_and_disable_usb_lpsc() != 0) {
			lpm_seq_trace_fail(LPM_SEQ_USB_LPSC_DISABLE);
			k3low_lpm_abort();
		} else {
			lpm_seq_trace(LPM_SEQ_USB_LPSC_DISABLE);
		}

		save_main_pll();
		lpm_seq_trace(LPM_SEQ_SAVE_MAIN_PLL);

		if (k3low_ddr_deep_sleep_suspend_sequence() != 0) {
			lpm_seq_trace_fail(LPM_SEQ_SAVE_DDR_REGS);
			k3low_lpm_abort();
		} else {
			lpm_seq_trace(LPM_SEQ_SAVE_DDR_REGS);
		}

		/* Disable the LPSCs for DDR */
		if (disable_ddr_lpsc() != 0) {
			lpm_seq_trace_fail(LPM_SEQ_DDR_LPSC_DISABLE);
			k3low_lpm_abort();
		} else {
			lpm_seq_trace(LPM_SEQ_DDR_LPSC_DISABLE);
		}

		/* configure the gpio clk input to 32K clock */
		config_gpio_clk_mux(WKUP_GPIO0_CLKSEL_CLK_32K);

		disable_main_pll(mode);
		lpm_seq_trace(LPM_SEQ_DISABLE_MAIN_PLL);

		dsb();
		isb();
		lpm_seq_trace(LPM_SEQ_BEFORE_WFI);

		for (;;) {
			wfi();
			lpm_seq_trace_fail(LPM_SEQ_UNEXPECTED_WFI_RETURN);
		}
	} else {
		for (;;) {
			lpm_seq_trace_fail(LPM_SEQ_INVALID_MODE);
		}
	}
}

/* Send core resume message to TIFS via the TX mailbox. */
static void mailbox_send_message(void)
{
	uint32_t *dst_ptr = (uint32_t *)(uintptr_t)(MAILBOX_TX_START_REGION +
						     MAILBOX_MSG_BUFFER_OFFSET);

	dst_ptr[0] = 0x0U;
	dst_ptr[1] = TISCI_MSG_CORE_RESUME;
	dst_ptr[2] = 0x0U;
	dst_ptr[3] = 0x0U;
	dst_ptr[4] = 0x0U;

	mmio_write_32(TI_MAILBOX_TX_BASE + TI_MAILBOX_MSG,
		      (uint32_t)(uintptr_t)dst_ptr);
}

void k3low_lpm_resume_c(void)
{
	if (restore_main_pll() != 0) {
		lpm_seq_trace_fail(LPM_SEQ_RESTORE_MAIN_PLL);
		k3low_lpm_abort();
	} else {
		lpm_seq_trace(LPM_SEQ_RESTORE_MAIN_PLL);
	}

	if (enable_ddr_lpsc() != 0) {
		lpm_seq_trace_fail(LPM_SEQ_ENABLE_DDR_LPSC);
		k3low_lpm_abort();
	} else {
		lpm_seq_trace(LPM_SEQ_ENABLE_DDR_LPSC);
	}

	if (k3low_ddr_deep_sleep_resume_sequence() != 0) {
		lpm_seq_trace_fail(LPM_SEQ_RESTORE_DDR_REGS);
		k3low_lpm_abort();
	} else {
		lpm_seq_trace(LPM_SEQ_RESTORE_DDR_REGS);
	}

	if (restore_usb_lpsc() != 0) {
		lpm_seq_trace_fail(LPM_SEQ_ENABLE_USB_LPSC);
		k3low_lpm_abort();
	} else {
		lpm_seq_trace(LPM_SEQ_ENABLE_USB_LPSC);
	}

	mailbox_send_message();
	lpm_seq_trace(LPM_SEQ_MAILBOX_SEND);

	for (;;) {
		wfi();
		lpm_seq_trace(LPM_SEQ_AFTER_WFI_RESUME);
	}
}
