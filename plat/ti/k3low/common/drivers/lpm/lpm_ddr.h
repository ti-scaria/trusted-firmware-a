/*
 * Copyright (C) 2014-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LPM_DDR_H
#define LPM_DDR_H

#include <plat/common/platform.h>

/*
 * Put DDR in self refresh for rtc only mode.
 *
 * Return 0 on success.
 */
int32_t k3low_put_ddr_in_rtc_lpm(void);

/*
 * Restore DDR controller context and take DDR out of self refresh.
 *
 * Return 0 on success.
 */
int32_t k3low_ddr_deep_sleep_resume_sequence(void);

/*
 * Save DDR register context, put DDR in self refresh and enable data retention.
 *
 * Return 0 on success.
 */
int32_t k3low_ddr_deep_sleep_suspend_sequence(void);

#endif /* LPM_DDR_H */
