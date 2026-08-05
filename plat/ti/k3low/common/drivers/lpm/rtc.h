/*
 * Copyright (c) 2024-2026, Texas Instruments Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LPM_RTC_H
#define LPM_RTC_H

#include <plat/common/platform.h>

struct rtc_time {
	uint32_t	sub_sec;
	uint32_t	sec_lo;
	uint32_t	sec_hi;
};

/*
 * Read the RTC counter registers into an rtc_time structure.
 *
 * rtc: pointer to rtc_time to populate; ignored if NULL.
 */
void k3low_lpm_rtc_read_time(struct rtc_time *rtc);

#endif /* LPM_RTC_H */
