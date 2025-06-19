/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Authors:
 *   Osman Büyükşar <osman.buyuksar@inventron.com.tr>
 */

#ifndef __SERIAL_TUBITAK_UART_H__
#define __SERIAL_TUBITAK_UART_H__

#include <sbi/sbi_types.h>

int tubitak_uart_init(unsigned long base, u32 in_freq, u32 baudrate);

#endif
