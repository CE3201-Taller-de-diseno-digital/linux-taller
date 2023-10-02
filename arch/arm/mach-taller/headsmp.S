/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  Copyright (c) 2003 ARM Limited
 *  Copyright (c) u-boot contributors
 *  Copyright (c) 2012 Pavel Machek <pavel@denx.de>
 *  Copyright (c) 2023 Alejandro Soto
 */
#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/memory.h>
#include <asm/assembler.h>

	.arch	armv4
	.arm

ENTRY(secondary_trampoline)
	/* CPUs 1-3 will always fetch from 0x0 when brought out of reset.
	 * Thus, we can just subtract the PAGE_OFFSET to get the physical
	 * address of secondary_startup. This would not work for platforms
	 * where the physical memory does not start at 0x0.
	*/
	adr	r0, 1f
	ldr r0, [r0]
	mov pc, r0

	.align
1:	.long	secondary_startup
ENTRY(secondary_trampoline_end)
