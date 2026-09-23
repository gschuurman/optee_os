PLATFORM_FLAVOR ?= axg

include core/arch/arm/cpu/cortex-armv8-0.mk

ifeq ($(PLATFORM_FLAVOR),g12b)
# A311D: 2x Cortex-A53 (MPIDR 0x000-0x001) + 4x Cortex-A73 (0x100-0x103)
$(call force,CFG_TEE_CORE_NB_CORE,6)
$(call force,CFG_CORE_CLUSTER_SHIFT,1)
# CFG_SHMEM_START/SIZE below (0x05000000, the static SHM pool) sits INSIDE a range BL2 hardware-secures --
# confirmed 2026-09-22 by decompiling bl2.bin: it programs the Amlogic AO secure-region protect registers
# with base=0x05000000 size=0x300000, and separately base=0x05300000 (CFG_TZDRAM_START) size=0x2000000,
# matching the DTS's own secmon@5000000 (3MiB) + secmon@5300000 (32MiB) reservations exactly. Two real,
# reproducible SError crashes came from normal-world writes into that first window (once via genalloc's
# own memset, once later via tee-supplicant's read() into an mmap'd buffer there -- SError is async/
# imprecise, so the fault can surface at an unrelated later instruction, which is why a preceding small
# allocation looked "fine"). This is not a sizing problem: no size in that range is genuinely safe.
#
# Real fix: CFG_CORE_DYN_SHM (default y) never actually took effect here, because OP-TEE only advertises
# OPTEE_SMC_SEC_CAP_DYNAMIC_SHM when core_mmu_nsec_ddr_is_defined() is true (core/arch/arm/tee/
# entry_fast.c), which requires the platform to register_ddr() its non-secure DDR ranges -- our g12b port
# never did. Now does, in main.c (explicitly excluding [0x05000000, 0x07300000), the full hardware-secure
# span). CFG_CORE_RESERVED_SHM is force-disabled below so Linux can never fall back to the static pool
# again even if dynamic-shm negotiation ever fails for some other reason.
# See device/khadas/vim3/handoff-keymint-optee.md.
$(call force,CFG_CORE_RESERVED_SHM,n)
# Real hardware RNG (rng.c, reads the same MMIO register Linux's meson-rng driver reads from
# normal world). A hang seen here earlier (2026-09-22) was misdiagnosed as a missing-clock-gate bus
# stall in rng.c; re-examining Linux's own meson-rng.c shows its clock is
# devm_clk_get_optional_enabled() -- optional -- so this chip variant has no gating dependency. The
# real cause was huk.c (a since-fixed unrelated RPC hang in early boot, confounded into every test
# that also had CFG_WITH_SOFTWARE_PRNG=n set); with that fixed, this reads clean on hardware.
$(call force,CFG_WITH_SOFTWARE_PRNG,n)
else
$(call force,CFG_TEE_CORE_NB_CORE,4)
endif

# CFG_TZDRAM_SIZE was 0x00c00000 (12 MiB) but BL2 hardware-protects the full 32 MiB window at
# CFG_TZDRAM_START (confirmed by decompiling bl2.bin, see the comment above on the AO secure-region
# protect registers) -- so 20 MiB of already-reserved, no-map secure DRAM sat unused. Use all of it:
# with CFG_WITH_PAGER=n TAs are fully resident, and the KeyMint TA alone is ~1.5 MiB plus a 4 MiB heap.
# No new hardware region or TF-A/BL2 change needed. (This was headroom, NOT the fix for the 2026-09-22
# KeyMint TEE_ERROR_OUT_OF_MEMORY -- that was a dangling TEEC_Context pointer in the HAL.)
CFG_TZDRAM_START ?= 0x05300000
CFG_TZDRAM_SIZE ?= 0x02000000
# CFG_SHMEM_START/SIZE intentionally NOT set for g12b: CFG_CORE_RESERVED_SHM=n above means they're unused,
# and the address range they'd default to is the proven-hardware-secure one described above. Do not set
# these for this platform without new hardware evidence that a specific range is genuinely safe.
ifneq ($(PLATFORM_FLAVOR),g12b)
CFG_SHMEM_START ?= 0x05000000
CFG_SHMEM_SIZE ?= 0x00100000
endif

$(call force,CFG_SECURE_TIME_SOURCE_CNTPCT,y)
$(call force,CFG_WITH_ARM_TRUSTED_FW,y)
$(call force,CFG_AMLOGIC_UART,y)

$(call force,CFG_WITH_PAGER,n)
$(call force,CFG_ARM64_core,y)
