/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * arch/arm/include/debug/taller.S
 *
 *  Copyright (C) 1994-2013 Russell King
 */

		.macro	addruart, rp, rv, tmp
		ldr	\rp, =0x30000000
		ldr	\rv, =0xf6000000
		.endm

		.macro	senduart,rd,rx
		str	\rd, [\rx]
		.endm

		.macro	busyuart,rd,rx
		.endm

		.macro	waituarttxrdy,rd,rx
1001:
		ldr	\rd, [\rx, #4]
		lsrs	\rd, \rd, #16
		beq	1001b
		.endm

		.macro	waituartcts,rd,rx
		.endm
