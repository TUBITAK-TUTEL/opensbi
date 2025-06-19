// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Inventron Electronics and Software
 * Author: Ceyhun Şen <ceyhun.sen@inventron.com.tr>
 */

#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/serial/sifive-uart.h>

// INTR_STATE
// INTR_ENABLE
// INTR_TEST
#define rx_parity_err 7
#define rx_timeout    6
#define rx_break_err  5
#define rx_frame_err  4
#define rx_overflow   3
#define tx_empty      2
#define rx_watermark  1
#define tx_watermark  0
// CTRL
#define NCO        16
#define RXBLVL     8
#define PARITY_ODD 7
#define PARITY_EN  6
#define LLPBK      5
#define SLPBK      4
#define NF         2
#define RX         1
#define TX         0
// STATUS
#define RXEMPTY 5
#define RXIDLE  4
#define TXIDLE  3
#define TXEMPTY 2
#define RXFULL  1
#define TXFULL  0
// RDATA
#define RDATA 0
// WDATA
#define WDATA 0
// FIFO_CTRL
#define TXILVL 5
#define RXILVL 2
#define TXRST  1
#define RXRST  0
// FIFO_STATUS
#define RXLVL 16
#define TXLVL 0
// OVRD
#define TXVAL 1
#define TXEN  0
// VAL
#define VAL_RX 0
// TIMEOUT_CTRL
#define EN  31
#define VAL 0

#define UART_RXFIFO_BITS (0x3F << RXLVL)
#define UART_TXFIFO_BITS (0x3F << TXLVL)

#define UART_TXFULL_MASK (1UL << TXFULL)
#define UART_RXFULL_MASK (1UL << RXFULL)
#define UART_TXEMPTY_MASK (1UL << TXEMPTY)
#define UART_RXEMPTY_MASK (1UL << RXEMPTY)

#define UART_NCO_MASK (0xff << NCO)

struct uart_tubitak_yonca {
	u32 intr_state;
	u32 intr_enable;
	u32 intr_test;
	u32 na;
	u32 ctrl;
	u32 status;
	u32 rdata;
	u32 wdata;
	u32 fifo_ctrl;
	u32 fifo_status;
	u32 ovrd;
	u32 val;
	u32 timeout_ctrl;
};

static struct tubitak_yonca_uart_plat {
	struct uart_tubitak_yonca *regs;
}yonca_uart_plat;


static inline u32 get_nco_bits(unsigned long f_clk, unsigned long f_baud)
{
	u32 nco;
	nco = 16 * (1 << 16) * f_baud / f_clk;

	return nco;
}

static int tubitak_yonca_serial_setbrg(u32 in_freq, u32 baudrate)
{
	u32 ctrl_val;

	ctrl_val = readl(&yonca_uart_plat.regs->ctrl);
	ctrl_val &= ~UART_NCO_MASK;
	ctrl_val |= (get_nco_bits(in_freq, baudrate)) << NCO;
	writel(ctrl_val, &yonca_uart_plat.regs->ctrl);

	return 0;
}


static int tubitak_yonca_serial_getc(void)
{
	int c;

	// Try again if RX FIFO is empty.
	if ((readl(&yonca_uart_plat.regs->fifo_status) & UART_RXFIFO_BITS) == 0)
		return -1;

	c = readl(&yonca_uart_plat.regs->rdata);

	return c;
}

static void tubitak_yonca_serial_putc(char ch)
{
	while ((readl(&yonca_uart_plat.regs->fifo_status) & UART_TXFIFO_BITS) >= 32){
		//busy wait
	}
	writel(ch, &yonca_uart_plat.regs->wdata);
	return;
}

static struct sbi_console_device tubitak_console = {
	.name = "tubitak_uart",
	.console_putc = tubitak_yonca_serial_putc,
	.console_getc = tubitak_yonca_serial_getc
};

int tubitak_uart_init(unsigned long base, u32 in_freq, u32 baudrate)
{
	yonca_uart_plat.regs = (struct uart_tubitak_yonca*)base;
	u32 reg_val;
	
	// Disable interrupts.
	writel(0, &yonca_uart_plat.regs->intr_enable);
	
	// Enable RX and TX.
	reg_val = (1 << RX) | (1 << TX);
	writel(reg_val, &yonca_uart_plat.regs->ctrl);

	if(in_freq && baudrate)
		tubitak_yonca_serial_setbrg(in_freq, baudrate);

	sbi_console_set_device(&tubitak_console);

	return 0;
}
