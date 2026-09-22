// SPDX-License-Identifier: BSD-2-Clause
/*
 * Hardware Unique Key derived from Amlogic's provisioned efuse AES-key field.
 *
 * g12b/A311D has a real hardware key ladder + 4Kbit EFUSE (32 x 128-bit blocks) per the A311D
 * datasheet (Sec 8.12: "Crypto" / "Key Ladder" / "EFUSE"), and "encrypted OTP" is listed as a
 * silicon security feature. The public datasheet doesn't document the key-ladder register
 * interface or any per-block lock bits, so driving that hardware directly isn't possible from
 * what's available here.
 *
 * What IS usable: reading efuse offset 0x20, size 0x20 (the "AES key" field, per community-mapped
 * S905X3/G12B-family efuse layout, corroborated on this exact device -- offsets 0x00-0x1f read as
 * chip-ID-like data matching AML_SM_GET_CHIP_ID, offsets 0x40-0x7f read as unprogrammed zero, and
 * 0x20-0x40 reads as a distinct, non-zero, non-chip-ID pattern, consistent with a real provisioned
 * key). This is genuine hardware-provisioned per-device key material, unlike the earlier
 * baked-into-firmware attempt (identical across every device sharing this build) or the earlier
 * per-boot RPC-persisted attempt (hangs boot -- SSK derivation runs via
 * service_init_late(tee_fs_init_key_manager) before any normal world exists to service RPC).
 *
 * Caveat, explicit and accepted: this field is NOT hardware-gated on this device. Our TF-A's SIP
 * handler (aml_sip_svc.c) applies no is_caller_secure() check to AML_SM_EFUSE_READ, so normal
 * world can read this exact field too (confirmed: read it from Linux userspace during bring-up).
 * Real hardware gating requires Amlogic's secure-boot/efuse-lock provisioning, which requires
 * Amlogic-signed boot images and is a one-way fuse blow -- out of scope, and not attempted here.
 * This HUK is therefore only as strong as root access to this device already is: no worse than
 * the CFG_INSECURE all-zero stub against a rooted attacker, but unlike the stub, unlike a value
 * baked into the firmware image, and unlike zero, it's a real, high-entropy, per-device secret
 * against everything short of that. Accepted as the practical ceiling on this hardware without
 * vendor NDA documentation for the key-ladder/lock-bit interface.
 *
 * The raw AES-key bytes are whitened through SHA-256 before use, so nothing derived from this
 * function ever equals the raw efuse content bit-for-bit.
 */

#include <crypto/crypto.h>
#include <kernel/panic.h>
#include <kernel/tee_common_otp.h>
#include <kernel/thread.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <stdint.h>
#include <string.h>
#include <string_ext.h>
#include <trace.h>
#include <util.h>

#define AML_SM_EFUSE_READ		0x82000030
#define AML_SM_GET_SHARE_MEM_OUTPUT_BASE 0x82000021

#define EFUSE_AES_KEY_OFFSET		0x20
#define EFUSE_AES_KEY_SIZE		0x20

/* g12b's fixed AML_SHARE_MEM_OUTPUT_BASE, per arm-trusted-firmware plat/amlogic/g12b/g12b_def.h.
 * Falls inside the hardware-secured DRAM span [0x05000000, 0x07300000) established via BL2
 * decompilation (see conf.mk) -- safe for OP-TEE (secure world) to map and read directly; that
 * protection is against *non-secure* access, which is why normal-world Linux instead has to
 * ioremap it after querying the address via AML_SM_GET_SHARE_MEM_OUTPUT_BASE.
 */
#define AML_SHARE_MEM_OUTPUT_BASE	0x050FF000

register_phys_mem_pgdir(MEM_AREA_IO_SEC, AML_SHARE_MEM_OUTPUT_BASE, CORE_MMU_PGDIR_SIZE);

TEE_Result tee_otp_get_hw_unique_key(struct tee_hw_unique_key *hwkey)
{
	struct io_pa_va pa_va = { .pa = AML_SHARE_MEM_OUTPUT_BASE };
	vaddr_t shm_va = io_pa_or_va(&pa_va, EFUSE_AES_KEY_SIZE);
	uint8_t digest[TEE_SHA256_HASH_SIZE] = { };
	void *hash_ctx = NULL;
	TEE_Result res = TEE_ERROR_GENERIC;
	unsigned long ret = 0;

	ret = thread_smc(AML_SM_EFUSE_READ, EFUSE_AES_KEY_OFFSET,
			 EFUSE_AES_KEY_SIZE, 0);
	if (!ret) {
		EMSG("huk: AML_SM_EFUSE_READ failed");
		return TEE_ERROR_SECURITY;
	}

	res = crypto_hash_alloc_ctx(&hash_ctx, TEE_ALG_SHA256);
	if (res)
		return res;
	res = crypto_hash_init(hash_ctx);
	if (res)
		goto out;
	res = crypto_hash_update(hash_ctx, (void *)shm_va, EFUSE_AES_KEY_SIZE);
	if (res)
		goto out;
	res = crypto_hash_final(hash_ctx, digest, sizeof(digest));
	if (res)
		goto out;

	COMPILE_TIME_ASSERT(sizeof(hwkey->data) <= sizeof(digest));
	memcpy(hwkey->data, digest, sizeof(hwkey->data));

out:
	crypto_hash_free_ctx(hash_ctx);
	memzero_explicit(digest, sizeof(digest));
	return res;
}
