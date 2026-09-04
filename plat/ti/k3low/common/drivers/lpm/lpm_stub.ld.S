/*
 * Copyright (C) 2024-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Standalone linker script for the AM62L LPM WKUP SRAM stub.
 *
 * SRAM layout:
 *   0x707f0000  .lpm_stub_resume   k3low_lpm_resume - boot ROM branches here
 *                                  on every warm wakeup; must be at SRAM base.
 *   ...         .lpm_stub_suspend  k3low_lpm_stub_entry
 *   ...         .lpm_stub_text     remaining stub code
 *   ...         .lpm_stub_data     initialised data; ends with lpm_stub_header
 *   ---         .lpm_stub_bss      NOLOAD
 *
 * The last 16 bytes of .lpm_stub_data are struct lpm_stub_header to locate
 * entry points and BSS location.
 */

#define DEVICE_WKUP_SRAM_BASE		0x707f0000
#define DEVICE_WKUP_SRAM_STACK_SIZE	0x1000
#define DEVICE_WKUP_SRAM_SIZE		0x00010000
#define DEVICE_WKUP_SRAM_CODE_SIZE	(DEVICE_WKUP_SRAM_SIZE - DEVICE_WKUP_SRAM_STACK_SIZE)
#define PAGE_SIZE			0x1000

MEMORY {
	SRAM (rwx) : ORIGIN = DEVICE_WKUP_SRAM_BASE,
	             LENGTH = DEVICE_WKUP_SRAM_CODE_SIZE
}

SECTIONS {
	. = DEVICE_WKUP_SRAM_BASE;

	/*
	 * Resume entry — must be first at SRAM base (0x707f0000).
	 * The boot ROM always branches to 0x707f0000 on warm wakeup.
	 */
	.lpm_stub_resume : {
		KEEP(*(.text.asm.k3low_lpm_resume))
		. = ALIGN(PAGE_SIZE);
	} >SRAM

	/* Suspend entry */
	.lpm_stub_suspend : {
		KEEP(*(.wkupsram.suspend_entry))
		. = ALIGN(PAGE_SIZE);
	} >SRAM

	.lpm_stub_text : {
		*(.text .text.*)
	} >SRAM

	.lpm_stub_data : ALIGN(8) {
		*(.data .data.*)
		*(.rodata .rodata.*)
		. = ALIGN(4);
		/* struct lpm_stub_header - offsets relative to SRAM base */
		LONG(ADDR(.lpm_stub_resume)  - DEVICE_WKUP_SRAM_BASE)  /* resume_offset  */
		LONG(ADDR(.lpm_stub_suspend) - DEVICE_WKUP_SRAM_BASE)  /* suspend_offset */
		LONG(ADDR(.lpm_stub_bss)     - DEVICE_WKUP_SRAM_BASE)  /* bss_offset     */
		LONG(SIZEOF(.lpm_stub_bss))                             /* bss_size       */
	} >SRAM

	/* BSS - NOLOAD */
	.lpm_stub_bss (NOLOAD) : ALIGN(8) {
		*(.bss .bss.* COMMON)
		. = ALIGN(8);
	} >SRAM

	ASSERT(ADDR(.lpm_stub_resume) == DEVICE_WKUP_SRAM_BASE,
	       "k3low_lpm_resume must be at WKUP SRAM base (0x707f0000)")
}
