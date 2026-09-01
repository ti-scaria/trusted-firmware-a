/*
 * Copyright (C) 2014-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LPM_TIMEOUT_H
#define LPM_TIMEOUT_H

#include <plat/common/platform.h>

/* Busy-wait approximately 1 microsecond using the system counter. */
void k3low_lpm_delay_1us(void);

#endif /* LPM_TIMEOUT_H */
