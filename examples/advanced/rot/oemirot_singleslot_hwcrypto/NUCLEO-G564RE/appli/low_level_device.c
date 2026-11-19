/**
  ******************************************************************************
  * @file    low_level_device.c
  * @brief   This file contains device definition for low_level_device
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025-2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include "low_level_flash.h"
#include "appli_flash_layout.h"

#if ((FLASH_SECONDARY_APP_SLOT_SIZE > 0U) || defined (MCUBOOT_PRIMARY_ONLY)) || \
    ((DATA_IMAGE_NUMBER == 1) && (FLASH_SECONDARY_DATA_SLOT_SIZE > 0U))
#if defined MCUBOOT_PRIMARY_ONLY
static flash_range_t ACCESS_VECT[] =
{
  { FLASH_PRIMARY_APP_SLOT_OFFSET + BANK_SIZE, FLASH_PRIMARY_APP_SLOT_OFFSET + FLASH_PRIMARY_APP_SLOT_SIZE + BANK_SIZE - 1 },
#if (DATA_IMAGE_NUMBER == 1)
  { FLASH_PRIMARY_DATA_SLOT_OFFSET + BANK_SIZE, FLASH_PRIMARY_DATA_SLOT_OFFSET + FLASH_PRIMARY_DATA_SLOT_SIZE + BANK_SIZE - 1 },
#endif /* (DATA_IMAGE_NUMBER == 1) */
};

#else
static flash_range_t ACCESS_VECT[] =
{
#if (FLASH_SECONDARY_APP_SLOT_SIZE > 0U)
  { FLASH_SECONDARY_APP_SLOT_OFFSET, FLASH_SECONDARY_APP_SLOT_OFFSET + FLASH_SECONDARY_APP_SLOT_SIZE - 1 },
#endif /* (FLASH_SECONDARY_APP_SLOT_SIZE > 0U) */
#if (DATA_IMAGE_NUMBER == 1)
  { FLASH_SECONDARY_DATA_SLOT_OFFSET, FLASH_SECONDARY_DATA_SLOT_OFFSET + FLASH_SECONDARY_DATA_SLOT_SIZE - 1 },
#endif /* (DATA_IMAGE_NUMBER == 1) */
};
#endif /* MCUBOOT_PRIMARY_ONLY */

#define ACCESS_VECT_NB (sizeof(ACCESS_VECT) / sizeof(flash_range_t))
#define ACCESS_VECT_RANGE ACCESS_VECT
#else
#define ACCESS_VECT_NB 0U
#define ACCESS_VECT_RANGE ((flash_range_t *)0)
#endif /* Secondary slot availability */

#if (INSTALLATION_MODE == INSTALLATION_MODE_SWAP) && ((FLASH_SECONDARY_APP_SLOT_SIZE > 0U)|| (defined MCUBOOT_PRIMARY_ONLY))

static flash_range_t WRITE_VECT[] =
{
#if (defined MCUBOOT_PRIMARY_ONLY)
  /* Areas for writing candidate images */
  { FLASH_PRIMARY_APP_SLOT_OFFSET + BANK_SIZE, FLASH_PRIMARY_APP_SLOT_OFFSET + FLASH_PRIMARY_APP_SLOT_SIZE + BANK_SIZE - 1 },
#if (DATA_IMAGE_NUMBER == 1)
  { FLASH_PRIMARY_DATA_SLOT_OFFSET + BANK_SIZE, FLASH_PRIMARY_DATA_SLOT_OFFSET + FLASH_PRIMARY_DATA_SLOT_SIZE + BANK_SIZE - 1 },
#endif /* (DATA_IMAGE_NUMBER == 1) */
#else
  /* Areas for writing candidate images */
  { FLASH_SECONDARY_APP_SLOT_OFFSET, FLASH_SECONDARY_APP_SLOT_OFFSET + FLASH_SECONDARY_APP_SLOT_SIZE - 1 },
#if (DATA_IMAGE_NUMBER == 1)
  { FLASH_SECONDARY_DATA_SLOT_OFFSET, FLASH_SECONDARY_DATA_SLOT_OFFSET + FLASH_SECONDARY_DATA_SLOT_SIZE - 1 },
#endif /* (DATA_IMAGE_NUMBER == 1) */
#endif /* (defined MCUBOOT_PRIMARY_ONLY) */
  
  /* Areas for writing confirm flag on installed image */
  { FLASH_PRIMARY_APP_CONFIRM_OFFSET, FLASH_PRIMARY_APP_CONFIRM_OFFSET + FLASH_PROG_SIZE - 1 },
#if (DATA_IMAGE_NUMBER == 1)
  { FLASH_PRIMARY_DATA_CONFIRM_OFFSET, FLASH_PRIMARY_DATA_CONFIRM_OFFSET + FLASH_PROG_SIZE - 1 },
#endif /* (DATA_IMAGE_NUMBER == 1) */
};
#define WRITE_VECT_NB (sizeof(WRITE_VECT) / sizeof(flash_range_t))
#define WRITE_VECT_RANGE WRITE_VECT
#else

#define WRITE_VECT_NB ACCESS_VECT_NB
#define WRITE_VECT_RANGE ACCESS_VECT_RANGE

#endif /* (INSTALLATION_MODE == INSTALLATION_MODE_SWAP) */

low_level_device_t FLASH0_DEV =
{
  .erase = { .nb = ACCESS_VECT_NB, .range = ACCESS_VECT_RANGE },
  .write = { .nb = WRITE_VECT_NB, .range = WRITE_VECT_RANGE },
};
