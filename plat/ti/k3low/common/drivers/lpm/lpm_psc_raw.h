/*
 * Copyright (C) 2014-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LPM_PSC_RAW_H
#define LPM_PSC_RAW_H

#include <plat/common/platform.h>

#define MDCTL_STATE_SWRSTDISABLE       0x00U
#define MDCTL_STATE_SYNCRST            0x01U
#define MDCTL_STATE_DISABLE            0x02U
#define MDCTL_STATE_ENABLE             0x03U
#define MDCTL_STATE_AUTO_SLEEP         0x04U
#define MDCTL_STATE_AUTO_WAKE          0x05U

#define PDCTL_STATE_OFF                 0U
#define PDCTL_STATE_ON                  1U

/*
 * Wait for a PSC power domain transition to complete.
 *
 * psc_base: base address of the PSC.
 * pd:       power domain index.
 * Return 0 on success, -ETIMEDOUT if the transition does not complete.
 */
int32_t k3low_psc_raw_pd_wait(uintptr_t psc_base, uint8_t pd);

/*
 * Initiate a PSC power domain transition.
 *
 * psc_base: base address of the PSC.
 * pd:       power domain index.
 */
void k3low_psc_raw_pd_initiate(uintptr_t psc_base, uint8_t pd);

/*
 * Set the next state for a power domain control register.
 *
 * psc_base: base address of the PSC.
 * pd:       power domain index.
 * state:    PDCTL_STATE_* value to program.
 * force:    true to set the FORCE bit.
 */
void k3low_psc_raw_pd_set_state(uintptr_t psc_base, uint8_t pd,
				uint32_t state, bool force);

/*
 * Set the next state for a local power sleep controller (LPSC).
 *
 * psc_base: base address of the PSC.
 * lpsc:     LPSC index.
 * state:    MDCTL_STATE_* value to program.
 * force:    true to set the FORCE bit.
 */
void k3low_psc_raw_lpsc_set_state(uintptr_t psc_base, uint8_t lpsc,
				  uint32_t state, bool force);

/*
 * Read the current state of an LPSC module from MDSTAT.
 *
 * psc_base: base address of the PSC.
 * lpsc:     LPSC index.
 * Return the current state field value.
 */
uint8_t k3low_psc_raw_lpsc_get_state(uintptr_t psc_base, uint8_t lpsc);

#endif /* LPM_PSC_RAW_H */
