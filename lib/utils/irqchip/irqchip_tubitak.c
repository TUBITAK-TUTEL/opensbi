
/*
 *
 * Authors:
 *   Osman Büyükşar <osman.buyuksar@inventron.com.tr>
 */

#include <sbi/riscv_io.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_string.h>
#include <sbi_utils/irqchip/irqchip_tubitak.h>
#include <sbi/sbi_irqchip.h>

/* Dynamic Configuration Parameters*/
// PLIC_TARGET_COUNT must be power of 2 and not bigger than 32
static uint32_t plic_target_count = 4;
static uint32_t plic_source_count = 40;
static uint32_t plic_priority_count = 16;

#define PLIC_TARGET_COUNT (plic_target_count)
#define PLIC_SOURCE_COUNT (plic_source_count)
#define PLIC_PRIORITY_COUNT (plic_priority_count)

#define LOG2(x) ((sizeof(unsigned int) * 8) - __builtin_clz(x) - 1)
#define NEXT_MULTIPLY_OF_4(x) \
    ({ \
    unsigned int _val = 4; \
    do{ \
        if(x > 0){ \
            _val = (((x)+3) & ~3); \
        } \
    }while(0); \
    _val; \
    }) \

#define PRIORITY_BIT_PER_SRC LOG2(PLIC_PRIORITY_COUNT)
#define ID_BIT_PER_TARGET LOG2(PLIC_SOURCE_COUNT + 1)
#define THRESHOLD_BIT_PER_TARGET (PRIORITY_BIT_PER_SRC)
#define IE_LEN_PER_TARGET (0x4UL * (1 + (PLIC_SOURCE_COUNT / 32)))

/* REG LEN */
#define PLIC_CONFIG_LEN (0x8)

#define PLIC_EL_LEN (NEXT_MULTIPLY_OF_4(1 * PLIC_SOURCE_COUNT / 8))

#define PLIC_PRIORITY_LEN (NEXT_MULTIPLY_OF_4(PLIC_SOURCE_COUNT * PRIORITY_BIT_PER_SRC / 8))

#define PLIC_IE_LEN (PLIC_TARGET_COUNT * NEXT_MULTIPLY_OF_4(PLIC_SOURCE_COUNT/8))

//we are making an assumption that the THRESHOLD_BIT_PER_TARGET is not bigger than 32
#define PLIC_THRESHOLD_LEN (PLIC_TARGET_COUNT * 0x4)

#define PLIC_ID_LEN (PLIC_TARGET_COUNT * NEXT_MULTIPLY_OF_4(ID_BIT_PER_TARGET/8))


/* REG OFFSET */
#define PLIC_CONFIG_OFFSET 0x0
#define PLIC_EL_OFFSET (PLIC_CONFIG_OFFSET + PLIC_CONFIG_LEN)

#define PLIC_PRIORITY_OFFSET (PLIC_EL_OFFSET + PLIC_EL_LEN)

#define PLIC_IE_OFFSET (PLIC_PRIORITY_OFFSET + PLIC_PRIORITY_LEN)

#define PLIC_THRESHOLD_OFFSET (PLIC_IE_OFFSET + PLIC_IE_LEN)

#define PLIC_ID_OFFSET (PLIC_THRESHOLD_OFFSET + PLIC_THRESHOLD_LEN)


/* REG FIELDS */
#define PLIC_CONFIG_HAS_THRESHOLD_OFFSET 48
#define PLIC_CONFIG_PRIORITIES_OFFSET 32
#define PLIC_CONFIG_TARGETS_OFFSET 16
#define PLIC_CONFIG_SOURCES_OFFSET 0

#define CONFIG_SOURCES_MASK (0xffff << (PLIC_CONFIG_SOURCES_OFFSET))
#define CONFIG_TARGETS_MASK (0xffff << (PLIC_CONFIG_TARGETS_OFFSET))
#define CONFIG_PRIORITIES_MASK (0x3fff << (PLIC_CONFIG_PRIORITIES_OFFSET))
#define CONFIG_HAS_THRESHOLD_MASK (1UL << (PLIC_CONFIG_HAS_THRESHOLD_OFFSET))

#define EL_OFF_FOR_SRC(SRC_ID) (PLIC_EL_OFFSET + \
        (0x4UL * (SRC_ID-1 / 32)))

#define EL_MASK_FOR_SRC(SRC_ID) (1UL << \
        ((SRC_ID-1) % 32))

#define PRI_PER_REG (32 / PRIORITY_BIT_PER_SRC)
#define PRIORITY_OFF_FOR_SRC(SRC_ID) PLIC_PRIORITY_OFFSET + \
        (0x4UL * ((SRC_ID) / PRI_PER_REG))

#define PRIORITY_SHIFT_FOR_SRC(SRC_ID) \
        ((((SRC_ID-1) % (PRI_PER_REG))) * (32/PRI_PER_REG))

#define PRIORITY_MASK(SRC_ID) (((1UL << PRIORITY_BIT_PER_SRC) - 1) << \
        PRIORITY_SHIFT_FOR_SRC(SRC_ID))

#define IE_OFF_FOR_SRC_AND_TARGET(SRC_ID,TARGET_ID) (PLIC_IE_OFFSET + \
        (TARGET_ID * IE_LEN_PER_TARGET ) + (0x4UL * (SRC_ID-1 / 32)))

#define IE_SHIFT_FOR_SRC(SRC_ID) ((SRC_ID-1) % 32)

#define IE_MASK_FOR_SRC(SRC_ID) (1UL << \
        ((SRC_ID-1) % 32))

#define THRESH_OFF_FOR_TARGET(TARGET_ID) (PLIC_THRESHOLD_OFFSET + \
        (0x4UL * TARGET_ID))

#define ID_OFF_FOR_TARGET(TARGET_ID) (PLIC_ID_OFFSET + \
        (0x4UL * TARGET_ID))

#define PLIC_CONTEXT_BASE 0x200000
#define PLIC_CONTEXT_STRIDE 0x1000


int tubitak_ic_external_irqfn()
{
    return SBI_ENODEV; // not implemented yet
}

static u32 tubitak_ic_get_priority_word_by_word(const struct tubitak_ic_data *tubitak_ic, u32 word_indx)
{
    volatile void* priority_addr = (char*)tubitak_ic->addr + PRIORITY_OFF_FOR_SRC(0) + 0x4 * word_indx;
    volatile u32 _val = readl(priority_addr);
    return _val; 
}

static void tubitak_ic_set_priority_word_by_word(const struct tubitak_ic_data *tubitak_ic, u32 val,
                 u32 word_indx)
{
    volatile void* priority = (char*)tubitak_ic->addr + PRIORITY_OFF_FOR_SRC(0) + 0x4 * word_indx;
    writel(val, priority);
}

void tubitak_ic_priority_save(const struct tubitak_ic_data *tubitak_ic, u32 *priority, u32 num)
{
	for (u32 i = 0; i < num; i++)
		priority[i] = tubitak_ic_get_priority_word_by_word(tubitak_ic, i);
}

void tubitak_ic_priority_restore(const struct tubitak_ic_data *tubitak_ic, const u32 *priority,
			   u32 num)
{
	for (u32 i = 0; i < num; i++)
		tubitak_ic_set_priority_word_by_word(tubitak_ic, i, priority[i]);
}

static u32 tubitak_ic_get_thresh(const struct tubitak_ic_data *tubitak_ic, u32 target_id)
{
    volatile void* target_thresh_addr = (void*)tubitak_ic->addr + THRESH_OFF_FOR_TARGET(target_id);
    u32 _val = readl(target_thresh_addr);
    return _val;
}

static void tubitak_ic_set_thresh(const struct tubitak_ic_data *tubitak_ic, u32 target_id, u32 val)
{
    volatile void* target_thresh_addr = (void*)tubitak_ic->addr + THRESH_OFF_FOR_TARGET(target_id);
    writel(val, target_thresh_addr);
}

/*static u32 tubitak_ic_get_ie(const struct tubitak_ic_data *tubitak_ic, u32 target_id,*/
/*		       u32 source_id)*/
/*{*/
/*    volatile void* src_ie_addr = (void*)tubitak_ic->addr + IE_OFF_FOR_SRC_AND_TARGET(source_id, target_id);*/
/*    volatile u32 _val = readl(src_ie_addr);*/
/*    _val &= IE_MASK_FOR_SRC(source_id) >> IE_SHIFT_FOR_SRC(source_id);*/
/*    return _val;*/
/*}*/

static u32 tubitak_ic_get_ie_word_by_word(const struct tubitak_ic_data *tubitak_ic, u32 target_id,
                u32 word_indx)
{
    volatile void* src_ie_addr = (void*)tubitak_ic->addr + IE_OFF_FOR_SRC_AND_TARGET(0, target_id) +
                                    0x4 * word_indx;
    volatile u32 _val = readl(src_ie_addr);
    return _val;
}

/*static void tubitak_ic_set_ie(const struct tubitak_ic_data *tubitak_ic, u32 target_id,*/
/*			u32 source_id, u32 val)*/
/*{*/
/*    volatile void* src_ie_addr = (void*)tubitak_ic->addr + IE_OFF_FOR_SRC_AND_TARGET(source_id, target_id);*/
/*    volatile u32 _val = val << IE_SHIFT_FOR_SRC(source_id);*/
/*    writel(_val, src_ie_addr);*/
/*}*/

static void tubitak_ic_set_ie_word_by_word(const struct tubitak_ic_data *tubitak_ic, u32 target_id,
            u32 word_indx, u32 val)
{
    volatile void* src_ie_addr = (void*)tubitak_ic->addr + IE_OFF_FOR_SRC_AND_TARGET(0, target_id) +
                                    0x4 * word_indx;
    writel(val, src_ie_addr);
}

void tubitak_ic_context_save(const struct tubitak_ic_data *tubitak_ic, int context_id,
		       u32 *enable, u32 *threshold, u32 num)
{
	u32 ie_words = tubitak_ic->num_src / 32 + 1;

	if (num > ie_words)
		num = ie_words;

	for (u32 i = 0; i < num; i++)
		enable[i] = tubitak_ic_get_ie_word_by_word(tubitak_ic, context_id, i);

	*threshold = tubitak_ic_get_thresh(tubitak_ic, context_id);
}

void tubitak_ic_context_restore(const struct tubitak_ic_data *tubitak_ic, int context_id,
			  const u32 *enable, u32 threshold, u32 num)
{
	u32 ie_words = tubitak_ic->num_src / 32 + 1;

	if (num > ie_words)
		num = ie_words;

	for (u32 i = 0; i < num; i++)
		tubitak_ic_set_ie_word_by_word(tubitak_ic, context_id, i, enable[i]);

	tubitak_ic_set_thresh(tubitak_ic, context_id, threshold);
}

int tubitak_ic_context_init(const struct tubitak_ic_data *tubitak_ic, int context_id,
		      bool enable, u32 threshold)
{
	u32 ie_words, ie_value;

	if (!tubitak_ic || context_id < 0)
		return SBI_EINVAL;

	ie_words = tubitak_ic->num_src / 32 + 1;
	ie_value = enable ? 0xffffffffU : 0U;

	for (u32 i = 0; i < ie_words; i++)
		tubitak_ic_set_ie_word_by_word(tubitak_ic, context_id, i, ie_value);

	tubitak_ic_set_thresh(tubitak_ic, context_id, threshold);

	return 0;
}

int tubitak_ic_warm_irqchip_init(const struct tubitak_ic_data *tubitak_ic,
			   int m_cntx_id, int s_cntx_id)
{
	int ret;

	/* By default, disable all IRQs for M-mode of target HART */
	if (m_cntx_id > -1) {
		ret = tubitak_ic_context_init(tubitak_ic, m_cntx_id, false, 0x7);
		if (ret)
			return ret;
	}

	/* By default, disable all IRQs for S-mode of target HART */
	if (s_cntx_id > -1) {
		ret = tubitak_ic_context_init(tubitak_ic, s_cntx_id, false, 0x7);
		if (ret)
			return ret;
	}

	return 0;
}

void tubitak_ic_get_and_set_plic_config(const struct tubitak_ic_data *tubitak_ic)
{
	u32 temp_value = 0;
	temp_value = readl((char*)tubitak_ic->addr + PLIC_CONFIG_OFFSET);
	temp_value = (temp_value & CONFIG_TARGETS_MASK) >> PLIC_CONFIG_TARGETS_OFFSET;
	if (temp_value % 4 != 0 | temp_value < 4) {
		sbi_printf("IRQCHIP: Invalid target count from plic config reg %d\n, using defaults", temp_value);
		return;
	}
	plic_target_count = temp_value;

	temp_value = readl((char*)tubitak_ic->addr + PLIC_CONFIG_OFFSET);
	temp_value = (temp_value & CONFIG_SOURCES_MASK) >> PLIC_CONFIG_SOURCES_OFFSET;
	if (temp_value < 2) {
		sbi_printf("IRQCHIP: Invalid source count from plic config reg %d\n", temp_value);
		return;
	}
	plic_source_count = temp_value;
	
	temp_value = readl((char*)tubitak_ic->addr + PLIC_CONFIG_OFFSET);
	temp_value = (temp_value & CONFIG_PRIORITIES_MASK) >> PLIC_CONFIG_PRIORITIES_OFFSET;
	if (temp_value % 4 != 0 || temp_value < 2){
		sbi_printf("IRQCHIP: Invalid priority count from plic config reg %d\n", temp_value);
		return;
	}
	plic_priority_count = temp_value;
}

int tubitak_ic_cold_irqchip_init(const struct tubitak_ic_data *tubitak_ic)
{
	int i;

	if (!tubitak_ic)
		return SBI_EINVAL;

	tubitak_ic_get_and_set_plic_config(tubitak_ic);
    sbi_irqchip_set_irqfn(tubitak_ic_external_irqfn);

	/* Configure default priorities of all IRQs */
	for (i = 1; i <= tubitak_ic->num_src; i++)
		tubitak_ic_set_priority_word_by_word(tubitak_ic, i, 0);

	return sbi_domain_root_add_memrange(tubitak_ic->addr, tubitak_ic->size, BIT(20),
					(SBI_DOMAIN_MEMREGION_MMIO |
					 SBI_DOMAIN_MEMREGION_SHARED_SURW_MRW));
}
