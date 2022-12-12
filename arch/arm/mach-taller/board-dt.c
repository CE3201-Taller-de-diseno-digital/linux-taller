// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#ifdef CONFIG_DEBUG_TALLER
static struct map_desc taller_io_desc[] __initdata = {
	{
		.virtual = 0xf6000000,
		.pfn = __phys_to_pfn(0x30000000),
		.length = SZ_4K,
		.type = MT_DEVICE,
	},
};
#endif

static void __init taller_map_io(void)
{
#ifdef CONFIG_DEBUG_TALLER
	iotable_init(taller_io_desc, ARRAY_SIZE(taller_io_desc));
#endif
}

static const char *taller_board_compat[] = {
	"itcr,ce3201",
	NULL,
};

DT_MACHINE_START(taller, "Proyecto Final CE3201")
	.map_io		= taller_map_io,
	.dt_compat	= taller_board_compat,
MACHINE_END
