/*
 * Copyright (C) 2014-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <lib/mmio.h>
#include <plat/common/platform.h>

#include <lpm_timeout.h>
#include <lpm_trace.h>

#define UART_16550_THR			0x00U
#define UART_16550_LSR			0x14U
/* Line Status Register bits */
#define UART_16550_LSR_SR_E		BIT(6)
#define UART_16550_LSR_TX_FIFO_E	BIT(5)
#define TRACE_HEXADECIMAL_BASE		16U
/*
 * TX FIFO timeout in 1 us poll intervals.  Chosen empirically to avoid
 * dropping characters at typical UART baud rates during LPM trace.
 */
#define K3_UART_TIMEOUT_TX		10000U

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

/* Write a single byte to the UART THR, polling the LSR for TX FIFO space. */
static void lpm_console_tx(uint8_t data)
{
	uint32_t val;
	uint32_t i = 0U;

	/*
	 * Poll the Line Status Register to ensure FIFO space is
	 * available before writing to avoid dropping chars.
	 */
	do {
		val = mmio_read_32(K3_WKUP_UART_BASE_ADDRESS +
				   UART_16550_LSR);
	} while ((i++ < K3_UART_TIMEOUT_TX) &&
		 ((val & UART_16550_LSR_TX_FIFO_E) == 0U));

	mmio_write_32(K3_WKUP_UART_BASE_ADDRESS + UART_16550_THR, data);
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
	lpm_console_tx((uint8_t)'0');
	lpm_console_tx((uint8_t)'x');

	/*
	 * Output string backwards: lpm_trace_int_to_hex stores digits
	 * from low to high, so we reverse here for correct display.
	 */
	for (i = 0U; i <= len; i++) {
		lpm_console_tx(str[len - i]);
	}

	/* Carriage return for terminals that require it. */
	lpm_console_tx((uint8_t)'\r');

	/* Move cursor to new line. */
	lpm_console_tx((uint8_t)'\n');
}

void k3low_lpm_trace_debug(uint32_t value)
{
	uint8_t str[9];
	uint8_t len;

	len = lpm_trace_int_to_hex(value, str);
	k3low_lpm_trace_debug_uart(str, len);
}
