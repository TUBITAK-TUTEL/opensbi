
/*
 *
 * Authors:
 *   Osman Büyükşar <osman.buyuksar@inventron.com.tr>
 */

#include <libfdt.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_heap.h>
#include <sbi/sbi_scratch.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/irqchip/fdt_irqchip.h>
#include <sbi_utils/irqchip/irqchip_tubitak.h>

static unsigned long tubitak_ic_ptr_offset;

#define tubitak_ic_get_hart_data_ptr(__scratch)				\
	sbi_scratch_read_type((__scratch), void *, tubitak_ic_ptr_offset)

#define tubitak_ic_set_hart_data_ptr(__scratch, __tubitak_ic)			\
	sbi_scratch_write_type((__scratch), void *, tubitak_ic_ptr_offset, (__tubitak_ic))

static unsigned long tubitak_ic_mtarget_offset;

#define tubitak_ic_get_hart_mcontext(__scratch)				\
	(sbi_scratch_read_type((__scratch), long, tubitak_ic_mtarget_offset) - 1)

#define tubitak_ic_set_hart_mcontext(__scratch, __mctx)			\
	sbi_scratch_write_type((__scratch), long, tubitak_ic_mtarget_offset, (__mctx) + 1)

static unsigned long tubitak_ic_starget_offset;

#define tubitak_ic_get_hart_scontext(__scratch)				\
	(sbi_scratch_read_type((__scratch), long, tubitak_ic_starget_offset) - 1)

#define tubitak_ic_set_hart_scontext(__scratch, __sctx)			\
	sbi_scratch_write_type((__scratch), long, tubitak_ic_starget_offset, (__sctx) + 1)

void fdt_tubitak_ic_priority_save(u8 *priority, u32 num)
{
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();

	tubitak_ic_priority_save(tubitak_ic_get_hart_data_ptr(scratch), priority, num);
}

void fdt_tubitak_ic_priority_restore(const u8 *priority, u32 num)
{
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();

	tubitak_ic_priority_restore(tubitak_ic_get_hart_data_ptr(scratch), priority, num);
}

void fdt_tubitak_ic_context_save(bool smode, u32 *enable, u32 *threshold, u32 num)
{
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();

	tubitak_ic_context_save(tubitak_ic_get_hart_data_ptr(scratch),
			  smode ? tubitak_ic_get_hart_scontext(scratch) :
				  tubitak_ic_get_hart_mcontext(scratch),
			  enable, threshold, num);
}

void fdt_tubitak_ic_context_restore(bool smode, const u32 *enable, u32 threshold,
			      u32 num)
{
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();

	tubitak_ic_context_restore(tubitak_ic_get_hart_data_ptr(scratch),
			     smode ? tubitak_ic_get_hart_scontext(scratch) :
				     tubitak_ic_get_hart_mcontext(scratch),
			     enable, threshold, num);
}

static int fdt_tubitak_ic_warm_init(void)
{
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();

	return tubitak_ic_warm_irqchip_init(tubitak_ic_get_hart_data_ptr(scratch),
				      tubitak_ic_get_hart_mcontext(scratch),
				      tubitak_ic_get_hart_scontext(scratch));
}

static int fdt_tubitak_ic_update_hartid_table(void *fdt, int nodeoff,
					    struct tubitak_ic_data *ic_data)
{
	const fdt32_t *val;
	u32 phandle, hwirq, hartid;
	struct sbi_scratch *scratch;
	int i, err, count, cpu_offset, cpu_intc_offset;

	val = fdt_getprop(fdt, nodeoff, "interrupts-extended", &count);
	if (!val || count < sizeof(fdt32_t))
		return SBI_EINVAL;
	count = count / sizeof(fdt32_t);

	for (i = 0; i < count; i += 2) {
		phandle = fdt32_to_cpu(val[i]);
		hwirq = fdt32_to_cpu(val[i + 1]);

		cpu_intc_offset = fdt_node_offset_by_phandle(fdt, phandle);
		if (cpu_intc_offset < 0)
			continue;

		cpu_offset = fdt_parent_offset(fdt, cpu_intc_offset);
		if (cpu_offset < 0)
			continue;

		err = fdt_parse_hart_id(fdt, cpu_offset, &hartid);
		if (err)
			continue;

		scratch = sbi_hartid_to_scratch(hartid);
		if (!scratch)
			continue;

		tubitak_ic_set_hart_data_ptr(scratch, ic_data);
		switch (hwirq) {
		case IRQ_M_EXT:
			tubitak_ic_set_hart_mcontext(scratch, i / 2);
			break;
		case IRQ_S_EXT:
			tubitak_ic_set_hart_scontext(scratch, i / 2);
			break;
		}
	}

	return 0;
}

static int fdt_tubitak_ic_cold_init(void *fdt, int nodeoff,
				  const struct fdt_match *match)
{
	int rc;
	struct tubitak_ic_data *ic_data;

	if (!tubitak_ic_ptr_offset) {
		tubitak_ic_ptr_offset = sbi_scratch_alloc_type_offset(void *);
		if (!tubitak_ic_ptr_offset)
			return SBI_ENOMEM;
	}

	if (!tubitak_ic_mtarget_offset) {
		tubitak_ic_mtarget_offset = sbi_scratch_alloc_type_offset(long);
		if (!tubitak_ic_mtarget_offset)
			return SBI_ENOMEM;
	}

	if (!tubitak_ic_starget_offset) {
		tubitak_ic_starget_offset = sbi_scratch_alloc_type_offset(long);
		if (!tubitak_ic_starget_offset)
			return SBI_ENOMEM;
	}

	ic_data = sbi_zalloc(sizeof(*ic_data));
	if (!ic_data)
		return SBI_ENOMEM;

	rc = fdt_parse_tubitak_ic_node(fdt, nodeoff, &ic_data->addr, &ic_data->size,
                                    &ic_data->num_targets, &ic_data->num_src);
	if (rc)
		goto fail_free_data;

	if (match->data) {
		void (*tubitak_ic_plat_init)(struct tubitak_ic_data *) = match->data;
		tubitak_ic_plat_init(ic_data);
	}

	rc = tubitak_ic_cold_irqchip_init(ic_data);
	if (rc)
		goto fail_free_data;

	rc = fdt_tubitak_ic_update_hartid_table(fdt, nodeoff, ic_data);
	if (rc)
		goto fail_free_data;

	return 0;

fail_free_data:
	sbi_free(ic_data);
	return rc;
}

static void tubitak_ic_plat_init(struct plic_data *pd)
{
    //initialize the actual hardware
}

static const struct fdt_match irqchip_plic_match[] = {
	{ .compatible = "tubitak,plic-1.0.0", 
	  .data = tubitak_ic_plat_init },
	{ /* sentinel */ }
};

struct fdt_irqchip fdt_irqchip_plic = {
	.match_table = irqchip_plic_match,
	.cold_init = fdt_tubitak_ic_cold_init,
	.warm_init = fdt_tubitak_ic_warm_init,
	.exit = NULL,
};
