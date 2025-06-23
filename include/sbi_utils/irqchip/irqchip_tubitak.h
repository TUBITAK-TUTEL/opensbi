
/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#ifndef __IRQCHIP_TUBITAK_H__
#define __IRQCHIP_TUBITAK_H__

#include <sbi/sbi_types.h>

struct tubitak_ic_data {
	unsigned long addr;
	unsigned long size;
	unsigned long num_src;
	unsigned long num_targets;
};

void tubitak_ic_priority_save(const struct tubitak_ic_data *tubitak_ic, u32 *priority, u32 num);

void tubitak_ic_priority_restore(const struct tubitak_ic_data *tubitak_ic, const u32 *priority,
			   u32 num);

void tubitak_ic_context_save(const struct tubitak_ic_data *tubitak_ic, int context_id,
		       u32 *enable, u32 *threshold, u32 num);

void tubitak_ic_context_restore(const struct tubitak_ic_data *tubitak_ic, int context_id,
			  const u32 *enable, u32 threshold, u32 num);

int tubitak_ic_context_init(const struct tubitak_ic_data *tubitak_ic, int context_id,
		      bool enable, u32 threshold);

int tubitak_ic_warm_irqchip_init(const struct tubitak_ic_data *tubitak_ic,
			   int m_cntx_id, int s_cntx_id);

int tubitak_ic_cold_irqchip_init(const struct tubitak_ic_data *tubitak_ic);

int tubitak_ic_external_irqfn();

#endif
