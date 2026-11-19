/*
 * Global configuration for the MCUboot and mbedTLS sources used by the
 * application project.
 */
#ifndef FW_UPDATE_PRE_INCLUDE_GLOBAL_H
#define FW_UPDATE_PRE_INCLUDE_GLOBAL_H

#define MCUBOOT_DOUBLE_SIGN_VERIF
#define MBEDTLS_CONFIG_FILE "mx_stfcf_config.h"

#include "generated/hal/stm32g5xx_hal_conf.h"
#include "mx_saes.h"
#include "mx_hash.h"
#include "mx_pka.h"
#include "mx_rng.h"

#define mx_stfcf_mbedtls_alt_hal_aes_init mx_saes_init
#define mx_stfcf_mbedtls_alt_hal_aes_deinit mx_saes_deinit
#define mx_stfcf_mbedtls_alt_hal_aes_gethandle mx_saes_gethandle
#define mx_stfcf_mbedtls_alt_hal_hash_init mx_hash_init
#define mx_stfcf_mbedtls_alt_hal_hash_deinit mx_hash_deinit
#define mx_stfcf_mbedtls_alt_hal_hash_gethandle mx_hash_gethandle
#define mx_stfcf_mbedtls_alt_hal_pka_init mx_pka_init
#define mx_stfcf_mbedtls_alt_hal_pka_deinit mx_pka_deinit
#define mx_stfcf_mbedtls_alt_hal_pka_gethandle mx_pka_gethandle
#define mx_stfcf_mbedtls_alt_hal_rng_init mx_rng_init
#define mx_stfcf_mbedtls_alt_hal_rng_deinit mx_rng_deinit
#define mx_stfcf_mbedtls_alt_hal_rng_gethandle mx_rng_gethandle

#endif /* FW_UPDATE_PRE_INCLUDE_GLOBAL_H */
