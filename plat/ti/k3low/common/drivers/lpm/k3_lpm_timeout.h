/*
 * Copyright (c) 2024-2026, Texas Instruments Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef K3LOW_LPM_TIMEOUT_H
#define K3LOW_LPM_TIMEOUT_H

#include <plat/common/platform.h>

/* Busy-wait approximately 1 microsecond using the system counter. */
__wkupsramfunc void k3low_lpm_delay_1us(void);

#endif /* K3LOW_LPM_TIMEOUT_H */
