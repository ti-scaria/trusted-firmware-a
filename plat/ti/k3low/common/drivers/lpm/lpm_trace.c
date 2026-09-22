/*
 * Copyright (C) 2024-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <lib/mmio.h>
#include <plat/common/platform.h>
#include <drivers/ti/uart/uart_16550.h>

#include <lpm_timeout.h>
#include <lpm_trace.h>

#define TRACE_HEXADECIMAL_BASE		16U

extern int console_16550_core_putc(int c, uintptr_t base_addr);

/*
 * Convert an integer value to a hexadecimal string, storing digits from
 * least-significant to most-significant nibble.
 *
 * value: input value to convert.
 * str:   output buffer for the hex digits (must be at least 9 bytes).
 * Return the index of the most-significant digit (string length minus 1).
 */
static uint8_t lpm_trace_int_to_hex(uint32_t value, uint8_t *str)
{
	uint32_t val_rem;
	uint8_t idx = 0U;

	if (value == 0U) {
		str[idx] = (uint8_t)'0';
		idx++;
	} else {
		while (value > 0U) {
			val_rem = value % TRACE_HEXADECIMAL_BASE;
			if (val_rem < 10U) {
				str[idx] = (uint8_t)(val_rem + (uint8_t)'0');
			} else {
				str[idx] = (uint8_t)((val_rem - 10U) +
						     (uint8_t)'A');
			}
			value /= TRACE_HEXADECIMAL_BASE;
			idx++;
		}
	}

	str[idx] = (uint8_t)'\0';

	if (idx > 1U) {
		/* Get length of string - NULL terminator */
		idx--;
	}

	return idx;
}

/*
 * Write a hex trace value to the UART.
 *
 * str: digits buffer (least-significant first) produced by lpm_trace_int_to_hex.
 * len: index of the most-significant digit.
 */
static void k3low_lpm_trace_debug_uart(uint8_t *str, uint8_t len)
{
	uint32_t i;

	/* Output "0x" prefix */
	console_16550_core_putc((int)'0', (uintptr_t)K3_WKUP_UART_BASE_ADDRESS);
	console_16550_core_putc((int)'x', (uintptr_t)K3_WKUP_UART_BASE_ADDRESS);

	/*
	 * Output string backwards: lpm_trace_int_to_hex stores digits
	 * from low to high, so we reverse here for correct display.
	 */
	for (i = 0U; i <= len; i++) {
		console_16550_core_putc((int)str[len - i], (uintptr_t)K3_WKUP_UART_BASE_ADDRESS);
	}

	/* Carriage return for terminals that require it. */
	console_16550_core_putc((int)'\r', (uintptr_t)K3_WKUP_UART_BASE_ADDRESS);

	/* Move cursor to new line. */
	console_16550_core_putc((int)'\n', (uintptr_t)K3_WKUP_UART_BASE_ADDRESS);
}

void k3low_lpm_trace_debug(uint32_t value)
{
	uint8_t str[9];
	uint8_t len;

	len = lpm_trace_int_to_hex(value, str);
	k3low_lpm_trace_debug_uart(str, len);
}
