/*
 * Copyright (C) 2014-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LPM_STUB_H
#define LPM_STUB_H

#include <plat/common/platform.h>

/* Low power mode resume C-level entry point (runs from WKUP SRAM). */
void k3low_lpm_resume_c(void);

/*
 * WKUP SRAM entry point for the A53 LPM stub.
 *
 * mode: LPM mode to enter.
 */
void k3low_lpm_stub_entry(uint32_t mode);

/* Abort the current LPM sequence with a trace event and spin. */
void k3low_lpm_abort(void);

#endif /* LPM_STUB_H */
