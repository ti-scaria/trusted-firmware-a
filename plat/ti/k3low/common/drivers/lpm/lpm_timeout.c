/*
 * Copyright (C) 2014-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <lpm_timeout.h>
#include <plat/common/platform.h>

void k3low_lpm_delay_1us(void)
{
	uint32_t tick_start = (uint32_t)read_cntpct_el0();
	uint32_t us_ticks = (SYS_COUNTER_FREQ_IN_TICKS / 1000000U);

	while (((uint32_t)read_cntpct_el0() - tick_start) < us_ticks) {
	}
}
