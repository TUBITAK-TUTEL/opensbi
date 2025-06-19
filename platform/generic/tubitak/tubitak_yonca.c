/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Authors:
 *      Osman Büyükşar
 */

#include <platform_override.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_fixup.h>

static const struct fdt_match tubitak_yonca_match[] = {
	{ .compatible = "tubitak,yonca" },
	{ },
};

const struct platform_override tubitak_yonca = {
	.match_table = tubitak_yonca_match
};
