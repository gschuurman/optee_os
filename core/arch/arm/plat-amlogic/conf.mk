PLATFORM_FLAVOR ?= axg

include core/arch/arm/cpu/cortex-armv8-0.mk

ifeq ($(PLATFORM_FLAVOR),g12b)
# A311D: 2x Cortex-A53 (MPIDR 0x000-0x001) + 4x Cortex-A73 (0x100-0x103)
$(call force,CFG_TEE_CORE_NB_CORE,6)
$(call force,CFG_CORE_CLUSTER_SHIFT,1)
else
$(call force,CFG_TEE_CORE_NB_CORE,4)
endif

CFG_TZDRAM_START ?= 0x05300000
CFG_TZDRAM_SIZE ?= 0x00c00000
CFG_SHMEM_START ?= 0x05000000
CFG_SHMEM_SIZE ?= 0x00100000

$(call force,CFG_SECURE_TIME_SOURCE_CNTPCT,y)
$(call force,CFG_WITH_ARM_TRUSTED_FW,y)
$(call force,CFG_AMLOGIC_UART,y)

$(call force,CFG_WITH_PAGER,n)
$(call force,CFG_ARM64_core,y)
