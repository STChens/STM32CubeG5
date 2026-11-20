/**
  ******************************************************************************
  * @file    mcuboot_shared_platform.c
  * @brief   MCUboot platform services backed by the OEMiRoT shared RAM block.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include <string.h>

#include "../shared/mcuboot_shared_data.h"
#include "boot_hal_hash_ref.h"
#include "bootutil/sign_key.h"
#include "bootutil/enc_key.h"
#include "bootutil_priv.h"
#include "plat_defs.h"
#include "plat_nv_counters.h"

obk_hdpl1_config_t OBK_Hdpl1Cfg;
obk_hdpl1_data_t OBK_data;

static uint8_t ImageValidHashUpdate;
static uint8_t ImageValidHashLoaded;
static uint8_t ImageValidHashRef[MCUBOOT_IMAGE_NUMBER * IMAGE_SHA_LENGTH];

unsigned int ecdsa_pub_key_hash_len = AUTH_PUB_KEY_HASH_LENGTH;
unsigned int pub_key_len;
const int bootutil_key_cnt = MCUBOOT_IMAGE_NUMBER;
unsigned int enc_priv_key_len = ENC_PRIV_KEY_LENGTH;

const struct bootutil_key bootutil_key_hash[] =
{
  {
    .key = OBK_Hdpl1Cfg.Hdpl3SecureAuthenticationPubKeyHash,
    .len = &ecdsa_pub_key_hash_len,
  },
#if (MCUBOOT_DATA_IMAGE_NUMBER == 1)
  {
    .key = OBK_Hdpl1Cfg.Hdpl3SecureAuthenticationPubKeyHash,
    .len = &ecdsa_pub_key_hash_len,
  },
#endif /* (MCUBOOT_DATA_IMAGE_NUMBER == 1) */
};

struct bootutil_key bootutil_keys[1] =
{
  {
    .key = NULL,
    .len = &pub_key_len,
  },
};

const struct bootutil_key bootutil_enc_key =
{
  .key = OBK_Hdpl1Cfg.Hdpl3EncryptionPrivKey,
  .len = &enc_priv_key_len,
};

static int32_t load_shared_obk_data(void)
{
  const volatile mcuboot_shared_data_t *shared = mcuboot_shared_data_get();

  if (shared == NULL)
  {
    return BOOT_EFLASH;
  }

  mcuboot_shared_copy_from((uint8_t *)&OBK_Hdpl1Cfg,
                           (const volatile uint8_t *)&shared->hdpl1_config,
                           sizeof(OBK_Hdpl1Cfg));
  mcuboot_shared_copy_from((uint8_t *)&OBK_data,
                           (const volatile uint8_t *)&shared->hdpl1_data,
                           sizeof(OBK_data));

  return 0;
}

static enum plat_err_t get_nv_counter_value(enum nv_counter_t counter_id,
                                            uint32_t *value)
{
  uint32_t current;
  uint32_t previous;

  switch (counter_id)
  {
    case BOOT_NV_COUNTER_0:
      current = OBK_data.Image0CurVersion;
      previous = OBK_data.Image0PrevVersion;
      break;

    case BOOT_NV_COUNTER_1:
      current = OBK_data.Image1CurVersion;
      previous = OBK_data.Image1PrevVersion;
      break;

    case BOOT_NV_COUNTER_2:
      current = OBK_data.Image2CurVersion;
      previous = OBK_data.Image2PrevVersion;
      break;

    case BOOT_NV_COUNTER_3:
      current = OBK_data.Image3CurVersion;
      previous = OBK_data.Image3PrevVersion;
      break;

    default:
      return PLAT_ERR_INVALID_INPUT;
  }

  if (current < previous)
  {
    return PLAT_ERR_SYSTEM_ERR;
  }

  *value = current;
  return PLAT_ERR_SUCCESS;
}

enum plat_err_t plat_init_nv_counter(void)
{
  return (load_shared_obk_data() == 0) ? PLAT_ERR_SUCCESS : PLAT_ERR_SYSTEM_ERR;
}

enum plat_err_t plat_read_nv_counter(enum nv_counter_t counter_id,
                                     uint32_t size,
                                     uint32_t *value)
{
  if ((value == NULL) || (size != sizeof(*value)))
  {
    return PLAT_ERR_INVALID_INPUT;
  }

  if (load_shared_obk_data() != 0)
  {
    return PLAT_ERR_SYSTEM_ERR;
  }

  if (get_nv_counter_value(counter_id, value) != PLAT_ERR_SUCCESS)
  {
    return PLAT_ERR_SYSTEM_ERR;
  }

  return (*value != UINT32_MAX) ? PLAT_ERR_SUCCESS : PLAT_ERR_SYSTEM_ERR;
}

enum plat_err_t plat_set_nv_counter(enum nv_counter_t counter_id,
                                    uint32_t value,
                                    uint32_t *updated)
{
  volatile mcuboot_shared_data_t *shared;
  enum plat_err_t status;
  uint32_t current;

  if (updated == NULL)
  {
    return PLAT_ERR_INVALID_INPUT;
  }

  *updated = 0U;
  if (value == UINT32_MAX)
  {
    return PLAT_ERR_MAX_VALUE;
  }

  if (load_shared_obk_data() != 0)
  {
    return PLAT_ERR_SYSTEM_ERR;
  }

  status = get_nv_counter_value(counter_id, &current);
  if (status != PLAT_ERR_SUCCESS)
  {
    return status;
  }

  if (value < current)
  {
    return PLAT_ERR_INVALID_INPUT;
  }

  if (value == current)
  {
    return PLAT_ERR_SUCCESS;
  }

  switch (counter_id)
  {
    case BOOT_NV_COUNTER_0:
      OBK_data.Image0PrevVersion = OBK_data.Image0CurVersion;
      OBK_data.Image0CurVersion = value;
      break;

    case BOOT_NV_COUNTER_1:
      OBK_data.Image1PrevVersion = OBK_data.Image1CurVersion;
      OBK_data.Image1CurVersion = value;
      break;

    case BOOT_NV_COUNTER_2:
      OBK_data.Image2PrevVersion = OBK_data.Image2CurVersion;
      OBK_data.Image2CurVersion = value;
      break;

    case BOOT_NV_COUNTER_3:
      OBK_data.Image3PrevVersion = OBK_data.Image3CurVersion;
      OBK_data.Image3CurVersion = value;
      break;

    default:
      return PLAT_ERR_INVALID_INPUT;
  }

  shared = mcuboot_shared_data_ptr();
  mcuboot_shared_copy_to((volatile uint8_t *)&shared->hdpl1_data,
                         (const uint8_t *)&OBK_data,
                         sizeof(OBK_data));
  *updated = 1U;
  return PLAT_ERR_SUCCESS;
}

int32_t boot_hash_ref_load(void)
{
  int32_t rc = load_shared_obk_data();

  if (rc == 0)
  {
    mcuboot_shared_copy_from(ImageValidHashRef,
                             (const volatile uint8_t *)&OBK_data.Image0SHA[0],
                             sizeof(ImageValidHashRef));
    ImageValidHashLoaded = 1U;
    ImageValidHashUpdate = 0U;
  }

  return rc;
}

int32_t boot_hash_ref_store(void)
{
  volatile mcuboot_shared_data_t *shared;

  if ((ImageValidHashLoaded == 0U) && (boot_hash_ref_load() != 0))
  {
    return BOOT_EFLASH;
  }

  shared = mcuboot_shared_data_ptr();
  if (mcuboot_shared_data_get() == NULL)
  {
    return BOOT_EFLASH;
  }

  if (ImageValidHashUpdate != 0U)
  {
    mcuboot_shared_copy_to((volatile uint8_t *)&OBK_data.Image0SHA[0],
                           ImageValidHashRef,
                           sizeof(ImageValidHashRef));
    mcuboot_shared_copy_to((volatile uint8_t *)&shared->hdpl1_data.Image0SHA[0],
                           ImageValidHashRef,
                           sizeof(ImageValidHashRef));
    ImageValidHashUpdate = 0U;
  }

  return 0;
}

int32_t boot_hash_ref_set(uint8_t *hash_ref, uint8_t size, uint8_t image_index)
{
  if ((hash_ref == NULL)
      || (size != IMAGE_SHA_LENGTH)
      || (image_index >= MCUBOOT_IMAGE_NUMBER)
      || ((ImageValidHashLoaded == 0U) && (boot_hash_ref_load() != 0)))
  {
    return BOOT_EFLASH;
  }

  (void)memcpy(&ImageValidHashRef[image_index * IMAGE_SHA_LENGTH],
               hash_ref,
               IMAGE_SHA_LENGTH);
  ImageValidHashUpdate = 1U;
  return 0;
}

int32_t boot_hash_ref_get(uint8_t *hash_ref, uint8_t size, uint8_t image_index)
{
  if ((hash_ref == NULL)
      || (size != IMAGE_SHA_LENGTH)
      || (image_index >= MCUBOOT_IMAGE_NUMBER)
      || ((ImageValidHashLoaded == 0U) && (boot_hash_ref_load() != 0)))
  {
    return BOOT_EFLASH;
  }

  (void)memcpy(hash_ref,
               &ImageValidHashRef[image_index * IMAGE_SHA_LENGTH],
               IMAGE_SHA_LENGTH);
  return 0;
}

int boot_retrieve_public_key_hash(uint8_t image_index,
                                  uint8_t *public_key_hash,
                                  size_t *key_hash_size)
{
  if ((public_key_hash == NULL)
      || (key_hash_size == NULL)
      || (*key_hash_size < AUTH_PUB_KEY_HASH_LENGTH)
      || (image_index >= MCUBOOT_IMAGE_NUMBER)
      || (load_shared_obk_data() != 0))
  {
    return -1;
  }

  mcuboot_shared_copy_from(public_key_hash,
                           OBK_Hdpl1Cfg.Hdpl3SecureAuthenticationPubKeyHash,
                           AUTH_PUB_KEY_HASH_LENGTH);
  *key_hash_size = AUTH_PUB_KEY_HASH_LENGTH;
  return 0;
}
