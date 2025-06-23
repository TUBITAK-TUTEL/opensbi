/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Authors:
 *      Osman Büyükşar
 */

#include <platform_override.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_fixup.h>

static u64 tubitak_yonca_tlbr_flush_limit(const struct fdt_match *match)
{

	return 0;
}

static int tubitak_yonca_fdt_fixup(void *fdt, const struct fdt_match *match)
{

	return 0;
}

static const struct fdt_match tubitak_yonca_match[] = {
	{ .compatible = "tubitak,yonca" },
	{ },
};

const struct platform_override tubitak_yonca = {
	.match_table = tubitak_yonca_match,
	.tlbr_flush_limit = tubitak_yonca_tlbr_flush_limit,
	.fdt_fixup = tubitak_yonca_fdt_fixup,
};
