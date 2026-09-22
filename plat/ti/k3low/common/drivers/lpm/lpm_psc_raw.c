/*
 * Copyright (C) 2024-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>

#include <lib/mmio.h>
#include <plat/common/platform.h>

#include <lpm_psc_raw.h>
#include <lpm_timeout.h>

#define PSC_PTCMD		0x120U
#define PSC_PTSTAT		0x128U
#define PSC_PDCTL(domain)	(0x300U + (4U * (domain)))
#define PSC_MDSTAT(id)		(0x800U + (4U * (id)))
#define PSC_MDCTL(id)		(0xa00U + (4U * (id)))

#define MDSTAT_STATE_MASK		GENMASK(5, 0)
#define MDCTL_STATE_MASK	GENMASK(5, 0)
#define MDCTL_FORCE		BIT(31)

#define PDCTL_STATE_MASK	BIT(0)
#define PDCTL_FORCE		BIT(31)
/*
 * Maximum number of 1 us poll iterations to wait for a PSC transition.
 * Chosen to be long enough for the slowest expected transition.
 */
#define PSC_TRANSITION_TIMEOUT		10000U

int32_t k3low_psc_raw_pd_wait(uintptr_t psc_base, uint8_t pd)
{
	uint32_t i;

	for (i = PSC_TRANSITION_TIMEOUT; i != 0U; i--) {
		if ((mmio_read_32(psc_base + PSC_PTSTAT) &
		     BIT(pd)) == 0U) {
			return 0;
		}
		k3low_lpm_delay_1us();
	}

	return -ETIMEDOUT;
}

void k3low_psc_raw_pd_initiate(uintptr_t psc_base, uint8_t pd)
{
	mmio_write_32(psc_base + PSC_PTCMD, BIT(pd));
}

void k3low_psc_raw_pd_set_state(uintptr_t psc_base, uint8_t pd,
				uint32_t state, bool force)
{
	uint32_t pdctl = mmio_read_32(psc_base + PSC_PDCTL(pd));

	pdctl &= ~PDCTL_STATE_MASK;
	pdctl |= state;

	if (force) {
		pdctl |= PDCTL_FORCE;
	} else {
		pdctl &= ~PDCTL_FORCE;
	}

	mmio_write_32(psc_base + PSC_PDCTL(pd), pdctl);
}

void k3low_psc_raw_lpsc_set_state(uintptr_t psc_base, uint8_t lpsc,
				  uint32_t state, bool force)
{
	uint32_t mdctl = mmio_read_32(psc_base + PSC_MDCTL(lpsc));

	mdctl &= ~MDCTL_STATE_MASK;
	mdctl |= state;

	if (force) {
		mdctl |= MDCTL_FORCE;
	} else {
		mdctl &= ~MDCTL_FORCE;
	}

	mmio_write_32(psc_base + PSC_MDCTL(lpsc), mdctl);
}

/*
 * Read the current LPSC state from MDSTAT.
 *
 * Returns the lower 6-bit state field.
 */
uint8_t k3low_psc_raw_lpsc_get_state(uintptr_t psc_base, uint8_t lpsc)
{
	return (uint8_t)(MDSTAT_STATE_MASK &
			 mmio_read_32(psc_base + PSC_MDSTAT(lpsc)));
}
