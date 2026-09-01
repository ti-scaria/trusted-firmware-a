/*
 * Copyright (c) 2024-2026, Texas Instruments Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * LPM control functions that run from DDR.
 *
 * These functions are called before jumping to WKUP SRAM - they set up the
 * system for suspend and copy the SRAM stub.
 */

#include <errno.h>

#include <bl31/bl31.h>
#include <board_def.h>
#include <common/debug.h>
#include <drivers/delay_timer.h>
#include <k3_lpm_ctrl.h>
#include <lib/mmio.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <lpm_stub.h>
#include <plat/common/platform.h>
#include <ti_sci.h>

#define WKUP0_EN				(0x4030U)
#define WKUP0_EN_ALL_SOURCES			(0x7FFFFU)
#define WKUP0_SRC				(0x4040U)
#define WKUP_CTRL_PMCTRL_IO_0			(0x84U)
#define WKUP_CTRL_PMCTRL_IO_1			(0x88U)
#define WKUP_CTRL_DEEPSLEEP_CTRL		(0x160U)
#define CANUART_WAKE_OFF_MODE			(0x1310U)
#define CANUART_WAKE_OFF_MODE_STAT1		(0x130CU)
#define CANUART_WAKE_OFF_MODE_STAT1_ENABLED	(0x1U)

#define WKUP_CTRL_DEEPSLEEP_CTRL_ENABLE_IO	(0x101U)
#define WKUP_CTRL_DEEPSLEEP_CTRL_DISABLE_IO	0U
#define WKUP_CTRL_PMCTRL_IO_ISOCLK_OVRD	BIT(0)
#define WKUP_CTRL_PMCTRL_IO_ISOOVR_EXTEND	BIT(4)
#define WKUP_CTRL_PMCTRL_IO_ISO_BYPASS	BIT(6)
#define WKUP_CTRL_PMCTRL_IO_WUCLK_CTRL	BIT(8)
#define WKUP_CTRL_PMCTRL_IO_IO_ISO_STATUS	BIT(25)
#define WKUP_CTRL_PMCTRL_IO_GLOBAL_WUEN	BIT(16)
#define WKUP_CTRL_PMCTRL_IO_IO_ISO_CTRL	BIT(24)
#define WKUP_CTRL_PMCTRL_IO_WRITE_MASK \
	(WKUP_CTRL_PMCTRL_IO_ISOCLK_OVRD	\
	 | WKUP_CTRL_PMCTRL_IO_ISOOVR_EXTEND	\
	 | WKUP_CTRL_PMCTRL_IO_ISO_BYPASS	\
	 | WKUP_CTRL_PMCTRL_IO_WUCLK_CTRL	\
	 | WKUP_CTRL_PMCTRL_IO_GLOBAL_WUEN	\
	 | WKUP_CTRL_PMCTRL_IO_IO_ISO_CTRL)
#define WKUP_CTRL_PMCTRL_IO_ISO_STATUS_TIMEOUT (1000U) /* 10ms */

#define RTC_ONLY_PLUS_DDR_MAGIC_WORD		(0x6D555555U)
#define DEEP_SLEEP_MAGIC_WORD			(0xD5555555U)

#define SCTLR_EL3_M_BIT				((uint32_t)1U << 0)

extern uint32_t k3low_lpm_switch_stack(uintptr_t jump, uintptr_t stack,
				       uint32_t arg);

#ifndef __ASSEMBLER__
IMPORT_SYM(unsigned long, __wkup_sram_start__, WKUP_SRAM_START);
IMPORT_SYM(unsigned long, __wkup_sram_end__, WKUP_SRAM_END);
IMPORT_SYM(unsigned long, __WKUP_SRAM_COPY_START__, WKUP_SRAM_COPY_START);
IMPORT_SYM(unsigned long, __wkup_sram_suspend_entry__, K3_SUSPEND_ENTRY);
IMPORT_SYM(unsigned long, __wkup_sram_bss_start__, WKUP_SRAM_BSS_START);
IMPORT_SYM(unsigned long, __wkup_sram_bss_end__, WKUP_SRAM_BSS_END);
#endif

void k3low_config_wake_sources(bool enable)
{
	uint32_t wake_up_src;

	if (enable) {
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE + WKUP0_EN,
			      WKUP0_EN_ALL_SOURCES);
	} else {
		wake_up_src = mmio_read_32(WKUP_CTRL_MMR_SEC_5_BASE +
					   WKUP0_SRC);
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE + WKUP0_EN, 0x00U);
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE + WKUP0_SRC,
			      wake_up_src);
	}
}

void k3low_lpm_config_magic_words(uint32_t mode)
{
	if (mode == TI_K3_SLEEP_MODE_RTC_PLUS_DDR) {
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE +
			      CANUART_WAKE_OFF_MODE,
			      RTC_ONLY_PLUS_DDR_MAGIC_WORD);
	} else {
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE +
			      CANUART_WAKE_OFF_MODE,
			      DEEP_SLEEP_MAGIC_WORD);
	}
}

int32_t k3low_lpm_set_io_isolation(bool enable)
{
	uint32_t timeout;
	uint32_t reg;
	int32_t ret;

	if (enable) {
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE +
			      WKUP_CTRL_DEEPSLEEP_CTRL,
			      WKUP_CTRL_DEEPSLEEP_CTRL_ENABLE_IO);

		/* Set global wuen */
		reg = mmio_read_32(WKUP_CTRL_MMR_SEC_5_BASE +
				   WKUP_CTRL_PMCTRL_IO_0);
		reg = reg & WKUP_CTRL_PMCTRL_IO_WRITE_MASK;
		reg = reg | WKUP_CTRL_PMCTRL_IO_GLOBAL_WUEN;
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE +
			      WKUP_CTRL_PMCTRL_IO_0, reg);

		reg = mmio_read_32(WKUP_CTRL_MMR_SEC_5_BASE +
				   WKUP_CTRL_PMCTRL_IO_1);
		reg = reg & WKUP_CTRL_PMCTRL_IO_WRITE_MASK;
		reg = reg | WKUP_CTRL_PMCTRL_IO_GLOBAL_WUEN;
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE +
			      WKUP_CTRL_PMCTRL_IO_1, reg);

		/* Set global isoin */
		reg = mmio_read_32(WKUP_CTRL_MMR_SEC_5_BASE +
				   WKUP_CTRL_PMCTRL_IO_0);
		reg = reg & WKUP_CTRL_PMCTRL_IO_WRITE_MASK;
		reg = reg | WKUP_CTRL_PMCTRL_IO_IO_ISO_CTRL;
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE +
			      WKUP_CTRL_PMCTRL_IO_0, reg);

		reg = mmio_read_32(WKUP_CTRL_MMR_SEC_5_BASE +
				   WKUP_CTRL_PMCTRL_IO_1);
		reg = reg & WKUP_CTRL_PMCTRL_IO_WRITE_MASK;
		reg = reg | WKUP_CTRL_PMCTRL_IO_IO_ISO_CTRL;
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE +
			      WKUP_CTRL_PMCTRL_IO_1, reg);

		/* Wait for IO isolation status on PMCTRL_IO_0 */
		ret = -ETIMEDOUT;
		for (timeout = WKUP_CTRL_PMCTRL_IO_ISO_STATUS_TIMEOUT;
		     timeout > 0U; timeout--) {
			reg = mmio_read_32(WKUP_CTRL_MMR_SEC_5_BASE +
					   WKUP_CTRL_PMCTRL_IO_0);
			if ((reg & WKUP_CTRL_PMCTRL_IO_IO_ISO_STATUS) ==
			    WKUP_CTRL_PMCTRL_IO_IO_ISO_STATUS) {
				ret = 0;
				break;
			}
			udelay(10);
		}
		if (ret != 0) {
			return ret;
		}

		/* Wait for IO isolation status on PMCTRL_IO_1 */
		ret = -ETIMEDOUT;
		for (timeout = WKUP_CTRL_PMCTRL_IO_ISO_STATUS_TIMEOUT;
		     timeout > 0U; timeout--) {
			reg = mmio_read_32(WKUP_CTRL_MMR_SEC_5_BASE +
					   WKUP_CTRL_PMCTRL_IO_1);
			if ((reg & WKUP_CTRL_PMCTRL_IO_IO_ISO_STATUS) ==
			    WKUP_CTRL_PMCTRL_IO_IO_ISO_STATUS) {
				ret = 0;
				break;
			}
			udelay(10);
		}
	} else {
		/* Clear global wuen */
		reg = mmio_read_32(WKUP_CTRL_MMR_SEC_5_BASE +
				   WKUP_CTRL_PMCTRL_IO_0);
		reg = reg & WKUP_CTRL_PMCTRL_IO_WRITE_MASK;
		reg = reg & (~WKUP_CTRL_PMCTRL_IO_GLOBAL_WUEN);
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE +
			      WKUP_CTRL_PMCTRL_IO_0, reg);

		reg = mmio_read_32(WKUP_CTRL_MMR_SEC_5_BASE +
				   WKUP_CTRL_PMCTRL_IO_1);
		reg = reg & WKUP_CTRL_PMCTRL_IO_WRITE_MASK;
		reg = reg & (~WKUP_CTRL_PMCTRL_IO_GLOBAL_WUEN);
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE +
			      WKUP_CTRL_PMCTRL_IO_1, reg);

		/* Clear global isoin */
		reg = mmio_read_32(WKUP_CTRL_MMR_SEC_5_BASE +
				   WKUP_CTRL_PMCTRL_IO_0);
		reg = reg & WKUP_CTRL_PMCTRL_IO_WRITE_MASK;
		reg = reg & (~WKUP_CTRL_PMCTRL_IO_IO_ISO_CTRL);
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE +
			      WKUP_CTRL_PMCTRL_IO_0, reg);

		reg = mmio_read_32(WKUP_CTRL_MMR_SEC_5_BASE +
				   WKUP_CTRL_PMCTRL_IO_1);
		reg = reg & WKUP_CTRL_PMCTRL_IO_WRITE_MASK;
		reg = reg & (~WKUP_CTRL_PMCTRL_IO_IO_ISO_CTRL);
		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE +
			      WKUP_CTRL_PMCTRL_IO_1, reg);

		mmio_write_32(WKUP_CTRL_MMR_SEC_5_BASE +
			      WKUP_CTRL_DEEPSLEEP_CTRL,
			      WKUP_CTRL_DEEPSLEEP_CTRL_DISABLE_IO);

		ret = 0;
	}
	return ret;
}

/* Jump to the LPM stub in WKUP SRAM with a temporary stack. */
static void k3_lpm_jump_to_stub(uint32_t mode)
{
	uintptr_t jump = (uintptr_t)K3_SUSPEND_ENTRY;
	uintptr_t stack = (uintptr_t)DEVICE_WKUP_SRAM_STACK_BASE;
	uint32_t sctlr;

	/* disable MMU */
	sctlr = (uint32_t)read_sctlr_el3();
	sctlr &= (uint32_t)~SCTLR_EL3_M_BIT;
	write_sctlr_el3((uint64_t)sctlr);

	k3low_lpm_switch_stack(jump, stack, mode);
}

void k3low_suspend_to_ram(uint32_t mode)
{
	k3_lpm_jump_to_stub(mode);
}

int32_t k3low_lpm_stub_copy_to_sram(void)
{
	const uintptr_t sram_base  = (uintptr_t)DEVICE_WKUP_SRAM_BASE;
	const uintptr_t sram_limit = sram_base + (uintptr_t)DEVICE_WKUP_SRAM_CODE_SIZE;
	const uintptr_t stub_start = (uintptr_t)WKUP_SRAM_COPY_START;
	const uintptr_t stub_len   = (uintptr_t)WKUP_SRAM_END -
				      (uintptr_t)WKUP_SRAM_START;
	const uintptr_t bss_start  = (uintptr_t)WKUP_SRAM_BSS_START;
	const uintptr_t bss_end    = (uintptr_t)WKUP_SRAM_BSS_END;
	const uintptr_t bss_len    = bss_end - bss_start;

	/*
	 * Check that the total SRAM footprint (code/data copy + BSS) fits
	 * within the code region.
	 */
	if ((sram_base + stub_len) > sram_limit) {
		ERROR("A53 stub size (0x%lx) exceeds SRAM code size (0x%lx)\n",
		      (unsigned long)stub_len,
		      (unsigned long)(sram_limit - sram_base));
		return -ERANGE;
	}
	if (bss_end > sram_limit) {
		ERROR("A53 stub BSS end (0x%lx) exceeds SRAM code limit (0x%lx)\n",
		      (unsigned long)bss_end,
		      (unsigned long)sram_limit);
		return -ERANGE;
	}

	/* Copy stub code and initialised data to SRAM */
	(void)memcpy((void *)sram_base, (const void *)stub_start, stub_len);
	flush_dcache_range(sram_base, stub_len);

	/*
	 * Zero the BSS region in SRAM.
	 */
	if (bss_len > 0U) {
		(void)memset((void *)bss_start, 0, bss_len);
		flush_dcache_range(bss_start, bss_len);
	}

	return 0;
}
