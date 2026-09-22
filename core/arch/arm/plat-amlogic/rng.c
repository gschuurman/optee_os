// SPDX-License-Identifier: BSD-2-Clause
/*
 * Amlogic G12B hardware RNG. This is the exact same MMIO register (0xff630218, the "meson-rng"
 * peripheral registered at meson-g12-common.dtsi's rng@218, absolute address = apb bus base
 * 0xff600000 + inner-bus offset 0x30000 + register offset 0x218) that Linux's
 * drivers/char/hw_random/meson-rng.c already reads successfully from *normal* world -- which
 * proves it isn't TrustZone-protected, so secure world can read it directly too. It's a
 * free-running "sample on read" register (not a FIFO), so concurrent reads from both worlds are
 * fine; each read just returns a fresh 32-bit sample.
 *
 * No CFG_WITH_SOFTWARE_PRNG fallback path exists for this: with CFG_WITH_SOFTWARE_PRNG=n (set in
 * conf.mk for g12b), core/crypto/rng_hw.c requires hw_get_random_bytes() to exist, which this
 * provides.
 */

#include <io.h>
#include <kernel/panic.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <rng_support.h>
#include <string.h>
#include <types_ext.h>
#include <util.h>

#define RNG_BASE	0xff630218
#define RNG_DATA	0x0000

register_phys_mem_pgdir(MEM_AREA_IO_SEC, RNG_BASE, CORE_MMU_PGDIR_SIZE);

static vaddr_t rng_base(void)
{
	static struct io_pa_va pa_va = { .pa = RNG_BASE };

	return io_pa_or_va(&pa_va, sizeof(uint32_t));
}

TEE_Result hw_get_random_bytes(void *buf, size_t blen)
{
	uint8_t *dst = buf;
	size_t left = blen;

	while (left) {
		uint32_t sample = io_read32(rng_base() + RNG_DATA);
		size_t n = MIN(left, sizeof(sample));

		memcpy(dst, &sample, n);
		dst += n;
		left -= n;
	}

	return TEE_SUCCESS;
}
