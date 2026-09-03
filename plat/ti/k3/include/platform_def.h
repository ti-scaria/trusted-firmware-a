/*
 * Copyright (c) 2017-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLATFORM_DEF_H
#define PLATFORM_DEF_H

#include <ti_platform_defs.h>

#if !K3_SEC_PROXY_LITE
#define SEC_PROXY_DATA_BASE	0x32C00000
#define SEC_PROXY_DATA_SIZE	0x80000
#define SEC_PROXY_SCFG_BASE	0x32800000
#define SEC_PROXY_SCFG_SIZE	0x80000
#define SEC_PROXY_RT_BASE	0x32400000
#define SEC_PROXY_RT_SIZE	0x80000
#else
#define SEC_PROXY_DATA_BASE	0x4D000000
#define SEC_PROXY_DATA_SIZE	0x80000
#define SEC_PROXY_SCFG_BASE	0x4A400000
#define SEC_PROXY_SCFG_SIZE	0x80000
#define SEC_PROXY_RT_BASE	0x4A600000
#define SEC_PROXY_RT_SIZE	0x80000
#endif /* K3_SEC_PROXY_LITE */

#define WKUP_CTRL_MMR0_BASE		UL(0x43000000)
#define WKUP_CTRL_MMR0_SIZE		UL(0x20000)
#define CTRLMMR_WKUP_JTAG_ID		(WKUP_CTRL_MMR0_BASE + 0x14)

#define JTAG_ID_PARTNO_WIDTH		U(0x10)
#define JTAG_ID_PARTNO_SHIFT		U(0xC)

#define JTAG_ID_PARTNO_AM62AX		U(0xBB8D)
#define JTAG_ID_PARTNO_AM62PX		U(0xBB9D)
#define JTAG_ID_PARTNO_AM62X		U(0xBB7E)
#define JTAG_ID_PARTNO_AM64X		U(0xBB38)
#define JTAG_ID_PARTNO_AM65X		U(0xBB5A)
#define JTAG_ID_PARTNO_J721E		U(0xBB64)
#define JTAG_ID_PARTNO_J7200		U(0xBB6D)
#define JTAG_ID_PARTNO_J721S2		U(0xBB75)
#define JTAG_ID_PARTNO_J722S		U(0xBBA0)
#define JTAG_ID_PARTNO_J784S4		U(0xBB80)

/* A-core Cluster Device ID for AM65x */
#define CLUSTER_DEVICE_START_ID_AM65X	U(198)

#define SEC_PROXY_TIMEOUT_US		1000000
#define SEC_PROXY_MAX_MESSAGE_SIZE	56

/*******************************************************************************
 * Memory layout constants
 ******************************************************************************/

/*
 * This RAM will be used for the bootloader including code, bss, and stacks.
 * It may need to be increased if BL31 grows in size.
 *
 * The link addresses are determined by BL31_BASE + offset.
 * When ENABLE_PIE is set, the TF images can be loaded anywhere, so
 * BL31_BASE is really arbitrary.
 *
 * When ENABLE_PIE is unset, BL31_BASE should be chosen so that
 * it matches to the physical address where BL31 is loaded, that is,
 * BL31_BASE should be the base address of the RAM region.
 *
 * Lets make things explicit by mapping BL31_BASE to 0x0 since ENABLE_PIE is
 * defined as default for our platform.
 */
#define BL31_BASE	UL(0x00000000) /* PIE remapped on fly */
#define BL31_SIZE	UL(0x00040000) /* 256k */
#define BL31_LIMIT	(BL31_BASE + BL31_SIZE)

#endif /* PLATFORM_DEF_H */
