// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2020 Carlo Caione <ccaione@baylibre.com>
 */

#include <console.h>
#include <drivers/amlogic_uart.h>
#include <kernel/boot.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <platform_config.h>
#include <stdint.h>

static struct amlogic_uart_data console_data;
register_phys_mem_pgdir(MEM_AREA_IO_SEC, CONSOLE_UART_BASE,
			CORE_MMU_PGDIR_SIZE);

/*
 * Non-secure DDR ranges, needed for core_mmu_nsec_ddr_is_defined() so OP-TEE actually offers
 * OPTEE_SMC_SEC_CAP_DYNAMIC_SHM to Linux (entry_fast.c gates this on it regardless of
 * CFG_CORE_DYN_SHM). Without this, Linux always falls back to the static CFG_SHMEM_* pool below
 * -- which, on this board, is INSIDE a range BL2 hardware-secures (confirmed by decompiling
 * bl2.bin: it programs the Amlogic AO secure-region protect registers with base=0x05000000
 * size=0x300000, covering CFG_SHMEM_START/SIZE in full). Real, reproducible SError crashes came
 * from writes into that region. Do not add register_ddr() ranges that overlap
 * [0x05000000, 0x07300000) -- that covers both the 3MiB "BL31/SHMEM" and 32MiB "BL32/TZDRAM"
 * secure windows BL2 configures (matching the DTS's own secmon@5000000 + secmon@5300000
 * reservations). 0xf5800000 is this unit's actual detected DDR size (BL2 serial log: "DDR size:
 * 3928MB") -- revisit if built for hardware with a different amount of RAM.
 * See device/khadas/vim3/handoff-keymint-optee.md.
 */
register_ddr(0x0, 0x05000000);
register_ddr(0x07300000, 0xf5800000 - 0x07300000);

void plat_console_init(void)
{
	amlogic_uart_init(&console_data, CONSOLE_UART_BASE);
	register_serial_console(&console_data.chip);
}
