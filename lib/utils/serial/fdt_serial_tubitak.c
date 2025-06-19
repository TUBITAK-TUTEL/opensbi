/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * 
 * Authors:
 *   Osman Büyükşar <osman.buyuksar@inventron.com.tr>
 */

#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/serial/fdt_serial.h>
#include <sbi_utils/serial/tubitak-uart.h>

static int serial_tubitak_init(void *fdt, int nodeoff,
				const struct fdt_match *match)
{
	int rc;
	struct platform_uart_data uart;

	rc = fdt_parse_tubitak_yonca_uart_node(fdt, nodeoff, &uart);
	if (rc)
		return rc;

	return tubitak_uart_init(uart.addr, uart.freq, uart.baud);
}

static const struct fdt_match serial_tubitak_match[] = {
	{ .compatible = "tubitakyonca,uart" },
	{ },
};

struct fdt_serial fdt_serial_tubitak = {
	.match_table = serial_tubitak_match,
	.init = serial_tubitak_init
};
