/*
 * Copyright (C) 2024-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <bl31/bl31.h>
#include <common/debug.h>
#include <drivers/delay_timer.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>
#include <ti_sci.h>

#include <board_def.h>
#include <k3_lpm_ctrl.h>

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
#define WKUP_CTRL_PMCTRL_IO_ISO_BYPASS		BIT(6)
#define WKUP_CTRL_PMCTRL_IO_WUCLK_CTRL		BIT(8)
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
#define WKUP_CTRL_PMCTRL_IO_ISO_STATUS_TIMEOUT	(1000U) /* 10ms */

#define RTC_ONLY_PLUS_DDR_MAGIC_WORD		(0x6D555555U)
#define DEEP_SLEEP_MAGIC_WORD			(0xD5555555U)

#define SCTLR_EL3_M_BIT				((uint32_t)1U << 0)

extern uint32_t k3low_lpm_switch_stack(uintptr_t jump, uintptr_t stack,
				       uint32_t arg);

/*
 * Symbols injected by objcopy -I binary when lpm_stub.bin is embedded into
 * BL31.
 */
extern const uint8_t _binary_lpm_stub_bin_start[];
extern const uint8_t _binary_lpm_stub_bin_end[];

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

/* Read the lpm_stub_header from the last 16 bytes of the in-BL31 blob. */
static void lpm_stub_read_header(struct lpm_stub_header *hdr)
{
	(void)memcpy(hdr,
		     (const void *)((uintptr_t)_binary_lpm_stub_bin_end -
				    sizeof(struct lpm_stub_header)),
		     sizeof(struct lpm_stub_header));
}

/*
 * Disable the EL3 MMU and jump to the LPM suspend entry in WKUP SRAM.
 * The stub runs with MMU off throughout suspend.
 */
static void k3_lpm_jump_to_stub(uint32_t mode)
{
	struct lpm_stub_header hdr;
	uintptr_t suspend_entry;
	const uintptr_t stack = (uintptr_t)DEVICE_WKUP_SRAM_STACK_BASE;
	uint32_t sctlr;

	lpm_stub_read_header(&hdr);
	suspend_entry = (uintptr_t)DEVICE_WKUP_SRAM_BASE + hdr.suspend_offset;

	sctlr = (uint32_t)read_sctlr_el3();
	sctlr &= (uint32_t)~SCTLR_EL3_M_BIT;
	write_sctlr_el3((uint64_t)sctlr);

	(void)k3low_lpm_switch_stack(suspend_entry, stack, mode);
}

void k3low_suspend_to_ram(uint32_t mode)
{
	k3_lpm_jump_to_stub(mode);
}

int32_t k3low_lpm_stub_copy_to_sram(void)
{
	const uintptr_t sram_base  = (uintptr_t)DEVICE_WKUP_SRAM_BASE;
	const uintptr_t sram_limit = sram_base +
				     (uintptr_t)DEVICE_WKUP_SRAM_CODE_SIZE;
	struct lpm_stub_header hdr;
	size_t bin_len;
	uintptr_t bss_start;
	uintptr_t bss_end;

	bin_len = (size_t)(_binary_lpm_stub_bin_end - _binary_lpm_stub_bin_start);

	if ((sram_base + bin_len) > sram_limit) {
		ERROR("LPM stub binary (0x%zx bytes) exceeds SRAM code region (0x%zx bytes)\n",
		      bin_len,
		      (size_t)(sram_limit - sram_base));
		return -ERANGE;
	}

	lpm_stub_read_header(&hdr);

	bss_start = sram_base + hdr.bss_offset;
	bss_end   = bss_start + hdr.bss_size;

	if (bss_end > sram_limit) {
		ERROR("LPM stub BSS (0x%x bytes at offset 0x%x) exceeds SRAM code region\n",
		      hdr.bss_size, hdr.bss_offset);
		return -ERANGE;
	}

	(void)memcpy((void *)sram_base, _binary_lpm_stub_bin_start, bin_len);
	flush_dcache_range(sram_base, bin_len);

	if (hdr.bss_size > 0U) {
		(void)memset((void *)bss_start, 0, hdr.bss_size);
		flush_dcache_range(bss_start, hdr.bss_size);
	}

	return 0;
}
