/*
 * Copyright (c) 2024-2026, Texas Instruments Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LPM_GTC_H
#define LPM_GTC_H

#include <plat/common/platform.h>

/*
 * Save GTC counter and disable GTC.
 *
 * Return 0 on success.
 */
int32_t k3low_lpm_sleep_suspend_gtc(void);

/*
 * Restore GTC counter and enable GTC.
 *
 * Return 0 on success.
 */
int32_t k3low_lpm_resume_gtc(void);

#endif /* LPM_GTC_H */
