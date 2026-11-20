/**
  ******************************************************************************
  * @file    fw_update_app.c
  * @brief   Firmware Update module.
  *          This file provides set of firmware functions to manage Firmware
  *          Update functionalities.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022-2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/

#include "com.h"
#include "common.h"
#include "ymodem.h"
#include "fw_update_app.h"
#include "Driver_Flash.h"
#include "stm32g5xx_hal_flash_itf.h"
#include <string.h>
#include <inttypes.h>
#include "appli_flash_layout.h"

#if defined MCUBOOT_PRIMARY_ONLY
#include "bootutil_priv.h"
#include "bootutil/security_cnt.h"
#include "boot_hal_imagevalid.h"
#include "boot_hal_hash_ref.h"
#include "mx_hash.h"
#include "mx_pka.h"
#include "mx_saes.h"
#include "psa/crypto.h"
#include "mbedtls/memory_buffer_alloc.h"

/* Allow to be customized by the project else the default value is applied */
#if !defined(ROT_MBEDTLS_MEM_BUF_LEN)
#ifdef MCUBOOT_ENCRYPT_RSA
#define ROT_MBEDTLS_MEM_BUF_LEN 0x2500
#else
#define ROT_MBEDTLS_MEM_BUF_LEN 0x2000
#endif /* MCUBOOT_ENCRYPT_RSA */
#endif /* !ROT_MBEDTLS_MEM_BUF_LEN */

/* Static buffer to be used by mbedtls for memory allocation */
#ifdef __ICCARM__
__NO_INIT static uint8_t mbedtls_mem_buf[ROT_MBEDTLS_MEM_BUF_LEN];
#else
static uint8_t mbedtls_mem_buf[ROT_MBEDTLS_MEM_BUF_LEN] __attribute__((section(".bss.NoInit"))) ;
#endif /* __ICCARM__ */

#if !defined(MCUBOOT_HW_ROLLBACK_PROT)
#error "Bank-switch image validation requires MCUboot hardware rollback protection"
#endif
#endif /* MCUBOOT_PRIMARY_ONLY */

#if (FLASH_SECONDARY_APP_SLOT_SIZE > 0U) || (defined MCUBOOT_PRIMARY_ONLY)

extern ARM_DRIVER_FLASH FLASH_PRIMARY_DEV_NAME;
extern ARM_DRIVER_FLASH FLASH_SECONDARY_DEV_NAME;
extern uint32_t total_size_received;

/* Private define ------------------------------------------------------------*/
static uint32_t m_uFileSizeYmodem = 0U;    /* !< Ymodem File size*/
static uint32_t m_uNbrBlocksYmodem = 0U;   /* !< Ymodem Number of blocks*/
static uint32_t m_uPacketsReceived = 0U;   /* !< Ymodem packets received*/
static uint32_t m_uFlashSectorSize = 0U;   /* !< Flash Sector Size */
static uint32_t m_uFlashMinWriteSize = 0U; /* !< FLash Min Write access*/

const uint32_t MagicTrailerValue[] =
{
  0xf395c277,
  0x7fefd260,
  0x0f505235,
  0x8079b62c,
};

#if defined MCUBOOT_PRIMARY_ONLY
static hal_status_t FW_UPDATE_ValidateBankImage(void);
static void FW_UPDATE_ValidateAndRequestBankSwitch(void);
static void FW_UPDATE_Request_BankSwitch(void);
#endif /* MCUBOOT_PRIMARY_ONLY */
static void FW_UPDATE_Request_AppImage_Install(void);
#if (DATA_IMAGE_NUMBER == 1)
static void FW_UPDATE_Request_DataImage_Install(void);
#endif /* (DATA_IMAGE_NUMBER == 1) */
static void FW_UPDATE_PrintWelcome(void);
static hal_status_t FW_UPDATE_DownloadNewFirmware(sfu_fw_image_flash_t *pFwImageDwlArea);
static hal_status_t FW_UPDATE_DownloadAppImage(void);
#if (DATA_IMAGE_NUMBER == 1)
static hal_status_t FW_UPDATE_DownloadDataImage(void);
#endif /* (DATA_IMAGE_NUMBER == 1) */
#if (INSTALLATION_MODE == INSTALLATION_MODE_SWAP)
static void FW_UPDATE_ValidAppImage(void);
#if (DATA_IMAGE_NUMBER == 1)
static void FW_UPDATE_ValidDataImage(void);
#endif /* (DATA_IMAGE_NUMBER == 1) */
#endif /* (INSTALLATION_MODE == INSTALLATION_MODE_SWAP) */

/**
  * @brief  Runs the firmware update menu processing.
  * @param  None
  * @retval None
  */
void FW_UPDATE_Run(void)
{
  uint8_t key = 0U;
  uint8_t exit = 0U;

  /* Print Firmware Update welcome message */
  FW_UPDATE_PrintWelcome();

  while (exit == 0U)
  {
    key = 0U;

    /* Clean the input path */
    COM_Flush();

    /* Receive key */
    if (COM_Receive(&key, 1U, RX_TIMEOUT) == HAL_OK)
    {
      (void)printf("%c\r\n\n", key);
      switch (key)
      {
        case '0' + MENU_NUM_DOWNLOAD_APP_IMAGE:
          FW_UPDATE_DownloadAppImage();
          break;

        case '0' + MENU_NUM_REQUEST_APP_IMAGE_INSTALL: 
#if defined MCUBOOT_PRIMARY_ONLY
          FW_UPDATE_ValidateAndRequestBankSwitch();
#else          
          FW_UPDATE_Request_AppImage_Install();
#endif
          break;
#if (INSTALLATION_MODE == INSTALLATION_MODE_SWAP)
        case '0' + MENU_NUM_VALIDATE_APP_IMAGE:
          FW_UPDATE_ValidAppImage();
          break;
#endif /* (INSTALLATION_MODE == INSTALLATION_MODE_SWAP) */
#if (DATA_IMAGE_NUMBER == 1)
        case '0' + MENU_NUM_DOWNLOAD_DATA_IMAGE:
          FW_UPDATE_DownloadDataImage();
          break;

        case '0' + MENU_NUM_REQUEST_DATA_IMAGE_INSTALL:
          FW_UPDATE_Request_DataImage_Install();
          break;
#if (INSTALLATION_MODE == INSTALLATION_MODE_SWAP)
        case '0' + MENU_NUM_VALIDATE_DATA_IMAGE:
          FW_UPDATE_ValidDataImage();
          break;
#endif /* (INSTALLATION_MODE == INSTALLATION_MODE_SWAP) */
#endif /* DATA_IMAGE_NUMBER == 1 */
        case '0' + MENU_NUM_RESET_INSTALLATION:
          printf("  -- Install image : reboot\r\n\n");
          NVIC_SystemReset();
          break;
        case 'x' :
          exit = 1U;
          break;
        default:
          printf("Invalid number !\r");
          break;
      }
      /* Print Main Menu message */
      FW_UPDATE_PrintWelcome();
    }
  }
}

/**
  * @brief  Run FW Update process.
  * @param  None
  * @retval HAL Status.
  */
static hal_status_t FW_UPDATE_DownloadAppImage(void)
{
  hal_status_t ret = HAL_ERROR;
  sfu_fw_image_flash_t fw_image_dwl_area;
  ARM_FLASH_INFO *data = FLASH_SECONDARY_DEV_NAME.GetInfo();

  /* Print Firmware Update welcome message */
  printf("  Download app image\r\n\r\n");

  /* Get Info about the download area */
#if (defined MCUBOOT_PRIMARY_ONLY)  
  /* Download to the same offset as primary slot in the 2nd bank*/
  fw_image_dwl_area.DownloadAddr =  FLASH_PRIMARY_APP_SLOT_OFFSET + BANK_SIZE;
  fw_image_dwl_area.MaxSizeInBytes = FLASH_PRIMARY_APP_SLOT_SIZE;
#else
  fw_image_dwl_area.DownloadAddr =  FLASH_SECONDARY_APP_SLOT_OFFSET;
  fw_image_dwl_area.MaxSizeInBytes = FLASH_SECONDARY_APP_SLOT_SIZE;
#endif
  fw_image_dwl_area.ImageOffsetInBytes = 0x0;
  m_uFlashSectorSize = data->sector_size;
  m_uFlashMinWriteSize = data->program_unit;

  /* Download new firmware image*/
  ret = FW_UPDATE_DownloadNewFirmware(&fw_image_dwl_area);

  if (HAL_OK == ret)
  {
    printf("  -- App image correctly downloaded \r\n\n");
    HAL_Delay(1000U);
  }

  return ret;
}

#if (DATA_IMAGE_NUMBER == 1)
/**
  * @brief  Run FW Update process.
  * @param  None
  * @retval HAL Status.
  */
static hal_status_t FW_UPDATE_DownloadDataImage(void)
{
  hal_status_t ret = HAL_ERROR;
  sfu_fw_image_flash_t fw_image_dwl_area;
  ARM_FLASH_INFO *data = FLASH_SECONDARY_DEV_NAME.GetInfo();

  /* Print Firmware Update welcome message */
  printf("  Download data image\r\n\r\n");

  /* Get Info about the download area */
  fw_image_dwl_area.DownloadAddr =  FLASH_SECONDARY_DATA_SLOT_OFFSET;
  fw_image_dwl_area.MaxSizeInBytes = FLASH_SECONDARY_DATA_SLOT_SIZE;
  fw_image_dwl_area.ImageOffsetInBytes = 0x0;
  m_uFlashSectorSize = data->sector_size;
  m_uFlashMinWriteSize = data->program_unit;

  /* Download new firmware image*/
  ret = FW_UPDATE_DownloadNewFirmware(&fw_image_dwl_area);

  if (HAL_OK == ret)
  {
    printf("  -- Data image correctly downloaded \r\n\n");
    HAL_Delay(1000U);
  }

  return ret;
}

/**
  * @brief  Write Magic Flag to trigger installation from Download Slot.
  * @brief Request installation' programs the MAGIC value at end of secondary slot.
  */
static void FW_UPDATE_Request_DataImage_Install(void)
{
  uint32_t MagicAddress = FLASH_SECONDARY_DATA_INSTALL_REQ_OFFSET;

  /* Write the magic to trigger installation at next reset */
  printf("  Write magic trailer at %" PRIx32 "\r\n\n", MagicAddress);

  if (FLASH_SECONDARY_DEV_NAME.ProgramData(MagicAddress, MagicTrailerValue, sizeof(MagicTrailerValue)) != ARM_DRIVER_OK)
  {
    printf(" Write magic trailer: failed \r\n\n");
  }
}
#endif /* (DATA_IMAGE_NUMBER == 1) */
#if (INSTALLATION_MODE == INSTALLATION_MODE_SWAP)
/**
  * @brief  Write Confirm Flag for the image.
  * @param  None
  * @retval None
  */
static void FW_UPDATE_ValidAppImage(void)
{
  const uint8_t FlagPattern[FLASH_PROG_SIZE] = {0x1 };
  const uint32_t ConfirmAddress = FLASH_PRIMARY_APP_CONFIRM_OFFSET;

  if (FLASH_PRIMARY_DEV_NAME.ProgramData(ConfirmAddress, FlagPattern, sizeof(FlagPattern)) == ARM_DRIVER_OK)
  {
    printf(
      "  --  Confirm flag correctly written %" PRIx32 " %" PRIx32 "\r\n\n",
      ConfirmAddress,
      (uint32_t)FlagPattern[0]);
  }
  else
  {
    printf("  -- Confirm flag not correctlty written \r\n\n");
  }
}

#if (DATA_IMAGE_NUMBER == 1)
static void FW_UPDATE_ValidDataImage(void)
{
  const uint8_t FlagPattern[FLASH_PROG_SIZE] = {0x1 };
  const uint32_t ConfirmAddress = FLASH_PRIMARY_DATA_CONFIRM_OFFSET;

  if (FLASH_PRIMARY_DEV_NAME.ProgramData(ConfirmAddress, FlagPattern, sizeof(FlagPattern)) == ARM_DRIVER_OK)
  {
    printf(
      "  --  Confirm flag correctly written %" PRIx32 " %" PRIx32 "\r\n\n",
      ConfirmAddress,
      (uint32_t)FlagPattern[0]);
  }
  else
  {
    printf("  -- Confirm flag not correctlty written \r\n\n");
  }
}
#endif /* (DATA_IMAGE_NUMBER == 1) */
#endif /* (INSTALLATION_MODE == INSTALLATION_MODE_SWAP) */
/**
  * @brief  Display the FW_UPDATE Main Menu choices on HyperTerminal.
  * @param  None.
  * @retval None.
  */
static void FW_UPDATE_PrintWelcome(void)
{
  printf("\r\n================ New firmware image =======================\r\n\n");
  printf("  Download app image ------------------------------------ %d\r\n\n", MENU_NUM_DOWNLOAD_APP_IMAGE);
#if (defined MCUBOOT_PRIMARY_ONLY) 
  printf("  Verify FW in second slot and swap if valid ------------ %d\r\n\n", MENU_NUM_REQUEST_APP_IMAGE_INSTALL);
#else
  printf("  Request installation of app image --------------------- %d\r\n\n", MENU_NUM_REQUEST_APP_IMAGE_INSTALL);
#endif
#if (INSTALLATION_MODE == INSTALLATION_MODE_SWAP)
  printf("  Validate app image ------------------------------------ %d\r\n\n", MENU_NUM_VALIDATE_APP_IMAGE);
#endif /* (INSTALLATION_MODE == INSTALLATION_MODE_SWAP)*/
#if (DATA_IMAGE_NUMBER == 1)
  printf("  Download data image ----------------------------------- %d\r\n\n", MENU_NUM_DOWNLOAD_DATA_IMAGE);
  printf("  Request installation of data image -------------------- %d\r\n\n", MENU_NUM_REQUEST_DATA_IMAGE_INSTALL);
#if (INSTALLATION_MODE == INSTALLATION_MODE_SWAP)
  printf("  Validate data image ----------------------------------- %d\r\n\n", MENU_NUM_VALIDATE_DATA_IMAGE);
#endif /* (INSTALLATION_MODE == INSTALLATION_MODE_SWAP) */
#endif /* (DATA_IMAGE_NUMBER == 1) */
  printf("  Reset to trigger installation ------------------------- %d\r\n\n", MENU_NUM_RESET_INSTALLATION);
  printf("  Previous menu ----------------------------------------- x\r\n\n");
  printf("  Selection : ");
}

/**
  * @brief  Write Magic Flag to trigger installation from Download Slot.
  * @brief Request installation' programs the MAGIC value at end of secondary slot.
  */
static void FW_UPDATE_Request_AppImage_Install(void)
{
  uint32_t MagicAddress = FLASH_SECONDARY_APP_INSTALL_REQ_OFFSET;

  /* Write the magic to trigger installation at next reset */
  printf("  Write magic trailer at %" PRIx32 "\r\n\n", MagicAddress);

  if (FLASH_SECONDARY_DEV_NAME.ProgramData(MagicAddress, MagicTrailerValue, sizeof(MagicTrailerValue)) != ARM_DRIVER_OK)
  {
    printf(" Write magic trailer: Failed \r\n\n");
  }
}

#if defined MCUBOOT_PRIMARY_ONLY
#if defined(MCUBOOT_DOUBLE_SIGN_VERIF)
uint8_t ImageValidEnable = 0U;
uint8_t ImageValidIndex = 0U;
uint8_t ImageValidStatus[MCUBOOT_IMAGE_NUMBER] = {0U};
#endif /* MCUBOOT_DOUBLE_SIGN_VERIF */

/**
  * @brief  Toggle the bank-swap option byte and reset the system.
  * @param  None
  * @retval None
  */
static void FW_UPDATE_Request_BankSwitch(void)
{
  hal_flash_itf_ob_bank_swap_t bank_swap;
  hal_flash_itf_ob_bank_swap_t requested_bank_swap;

  printf("  Request bank switch\r\n\n");

  if (HAL_FLASH_ITF_OB_IsLockedBankSwap(HAL_FLASH) != HAL_FLASH_ITF_OB_BANK_SWAP_UNLOCKED)
  {
    printf("  -- Bank swap is locked; cannot change option byte\r\n\n");
    return;
  }

  if ((HAL_FLASH_ITF_Unlock(HAL_FLASH) != HAL_OK)
      || (HAL_FLASH_ITF_OB_Unlock(HAL_FLASH) != HAL_OK))
  {
    printf("  -- Failed to unlock Flash option-byte access\r\n\n");
    return;
  }

  bank_swap = HAL_FLASH_ITF_OB_GetBankSwap(HAL_FLASH);
  requested_bank_swap = (bank_swap == HAL_FLASH_ITF_OB_BANK_SWAP) ?
                        HAL_FLASH_ITF_OB_BANK_NO_SWAP : HAL_FLASH_ITF_OB_BANK_SWAP;

  if (HAL_FLASH_ITF_OB_SetBankSwap(HAL_FLASH, requested_bank_swap) != HAL_OK)
  {
    printf("  -- Failed to set bank-swap option byte\r\n\n");
    return;
  }

  if (HAL_FLASH_ITF_OB_Program(HAL_FLASH) != HAL_OK)
  {
    printf("  -- Failed to program Flash option bytes\r\n\n");
    return;
  }

  printf("  -- Bank-swap option byte updated; resetting system\r\n\n");
  NVIC_SystemReset();
}

/**
  * @brief  Validate the firmware in the other bank using MCUboot validation.
  * @param  None
  * @retval HAL status
  */
static hal_status_t FW_UPDATE_ValidateBankImage(void)
{
  static uint8_t tmp_buf[BOOT_TMPBUF_SZ];
  const struct flash_area *primary_fap = NULL;
  struct flash_area bank_fap;
  struct image_header image_hdr;
  FIH_DECLARE(fih_rc, FIH_FAILURE);

  if (flash_area_open(FLASH_AREA_IMAGE_PRIMARY(0), &primary_fap) != 0)
  {
    printf("  -- Failed to open the primary flash area\r\n\n");
    return HAL_ERROR;
  }

  bank_fap = *primary_fap;
  flash_area_close(primary_fap);
  bank_fap.fa_id = FLASH_AREA_IMAGE_SECONDARY(0);
  bank_fap.fa_driver = &FLASH_SECONDARY_DEV_NAME;
  bank_fap.fa_off = FLASH_PRIMARY_APP_SLOT_OFFSET + BANK_SIZE;
  bank_fap.fa_size = FLASH_PRIMARY_APP_SLOT_SIZE;

  if (flash_area_read(&bank_fap, 0U, &image_hdr, sizeof(image_hdr)) != 0)
  {
    printf("  -- Failed to read the firmware header from the other bank\r\n\n");
    return HAL_ERROR;
  }

  if ((image_hdr.ih_magic != IMAGE_MAGIC)
      || (image_hdr.ih_hdr_size > bank_fap.fa_size)
      || (image_hdr.ih_img_size >= (bank_fap.fa_size - image_hdr.ih_hdr_size))
      || ((image_hdr.ih_flags & IMAGE_F_NON_BOOTABLE) != 0U)
      /*|| IS_ENCRYPTED(&image_hdr)*/)
  {
    printf("  -- Firmware header is invalid or not bank-switchable\r\n\n");
    return HAL_ERROR;
  }

  FIH_CALL(boot_nv_security_counter_init, fih_rc);
  if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS))
  {
    printf("  -- Failed to initialize the anti-rollback counter\r\n\n");
    return HAL_ERROR;
  }

  if (boot_hash_ref_load() != 0)
  {
    printf("  -- Failed to load hash references from shared RAM\r\n\n");
    return HAL_ERROR;
  }

  /* Initialise the mbedtls static memory allocator so that mbedtls allocates
   * memory from the provided static buffer instead of from the heap.
   */
  mbedtls_memory_buffer_alloc_init(mbedtls_mem_buf, ROT_MBEDTLS_MEM_BUF_LEN);

  if (psa_crypto_init() != PSA_SUCCESS)
  {
    printf("  -- Failed to initialize PSA Crypto\r\n\n");
    return HAL_ERROR;
  }

  ImageValidEnable = 0U;
  ImageValidIndex = 0U;
  FIH_CALL(bootutil_img_validate, fih_rc, NULL, 0, &image_hdr, &bank_fap,
           tmp_buf, sizeof(tmp_buf), NULL, 0, NULL);
  if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS))
  {
    printf("  -- MCUboot firmware validation failed (signature, hash, or anti-rollback)\r\n\n");
    return HAL_ERROR;
  }

  return HAL_OK;
}

/**
  * @brief  Validate the firmware in the other bank and switch to it if valid.
  * @param  None
  * @retval None
  */
static void FW_UPDATE_ValidateAndRequestBankSwitch(void)
{
  printf("  Verify firmware in the other bank\r\n\n");

  if (FW_UPDATE_ValidateBankImage() != HAL_OK)
  {
    printf("  -- Firmware verification failed; bank switch cancelled\r\n\n");
    return;
  }

  printf("  -- Firmware verified successfully\r\n\n");
  FW_UPDATE_Request_BankSwitch();
}
#endif /* MCUBOOT_PRIMARY_ONLY */

/**
  * @brief Download a new Firmware from the host.
  * @retval HAL status
  */
static hal_status_t FW_UPDATE_DownloadNewFirmware(sfu_fw_image_flash_t *pFwImageDwlArea)
{
  hal_status_t ret = HAL_ERROR;
  COM_StatusTypeDef e_result = COM_ERROR;
  int32_t ret_arm = 0;
  uint32_t u_fw_size = pFwImageDwlArea->MaxSizeInBytes ;
  uint32_t sector_address = 0U;

  /* Clear download area */
  printf("  -- Erasing download area \r\n\n");

  for (sector_address = pFwImageDwlArea->DownloadAddr;
       sector_address < pFwImageDwlArea->DownloadAddr + pFwImageDwlArea->MaxSizeInBytes;
       sector_address += m_uFlashSectorSize)
  {
    ret_arm = FLASH_SECONDARY_DEV_NAME.EraseSector(sector_address);
    if (ret_arm < 0)
    {
      return HAL_ERROR;
    }
  }

  printf("  -- Send firmware \r\n\n");

  /* Download binary */
  printf("  -- -- File> Transfer> YMODEM> Send ");

  /*Init of Ymodem*/
  Ymodem_Init();

  /*Receive through Ymodem*/
  e_result = Ymodem_Receive(&u_fw_size, pFwImageDwlArea->DownloadAddr);
  printf("\r\n");

  if (e_result == COM_OK)
  {
    printf("  -- -- Programming completed successfully!\r\n\n");
    printf("  -- -- Bytes: %" PRIu32 "\r\n\n", u_fw_size);
    ret = HAL_OK;
  }
  else if (e_result == COM_ABORT)
  {
    printf("  -- -- !!Aborted by user!!\r\n\n");
    COM_Flush();
    ret = HAL_ERROR;
  }
  else
  {
    printf("  -- -- !!Error during file download!!\r\n\n");
    ret = HAL_ERROR;
    HAL_Delay(500U);
    COM_Flush();
  }

  return ret;
}

/**
  * @brief  Ymodem Header Packet Transfer completed callback.
  * @param  uFileSize Dimension of the file that will be received (Bytes).
  * @retval None
  */
hal_status_t Ymodem_HeaderPktRxCpltCallback(uint32_t uFlashDestination, uint32_t uFileSize)
{
  /*Reset of the ymodem variables */
  m_uFileSizeYmodem = 0U;
  m_uPacketsReceived = 0U;
  m_uNbrBlocksYmodem = 0U;

  /*Filesize information is stored*/
  m_uFileSizeYmodem = uFileSize;

  /* compute the number of 1K blocks */
  m_uNbrBlocksYmodem = (m_uFileSizeYmodem + (PACKET_1K_SIZE - 1U)) / PACKET_1K_SIZE;

  /* NOTE : delay inserted for Ymodem protocol*/
  HAL_Delay(1000);
  return HAL_OK;
}

/**
  * @brief  Ymodem Data Packet Transfer completed callback.
  * @param  pData Pointer to the buffer.
  * @param  uSize Packet dimension (Bytes).
  * @retval None
  */
hal_status_t Ymodem_DataPktRxCpltCallback(uint8_t *pData, uint32_t uFlashDestination, uint32_t uSize)
{
  int32_t ret = HAL_ERROR;

  /* Increase the number of received packets
  Increment the packet counter first, then check whether it is the last one. */
  m_uPacketsReceived++;

  if (m_uPacketsReceived == m_uNbrBlocksYmodem) /*Last Packet*/
  {
    /*Extracting actual payload from last packet*/
    if (0U == (m_uFileSizeYmodem % PACKET_1K_SIZE))
    {
      /* The last packet must be fully considered */
      uSize = PACKET_1K_SIZE;
    }
    else
    {
      /* The last packet is not full, drop the extra bytes */
      uSize = m_uFileSizeYmodem - ((uint32_t)(m_uFileSizeYmodem / PACKET_1K_SIZE) * PACKET_1K_SIZE);
    }

    m_uPacketsReceived = 0U;
  }
  /*Adjust dimension to 64-bit length */
  if ((uSize %  m_uFlashMinWriteSize) != 0U)
  {
    memset(&pData[uSize], 0xff, (m_uFlashMinWriteSize - (uSize %  m_uFlashMinWriteSize)));
    uSize += (m_uFlashMinWriteSize - (uSize %  m_uFlashMinWriteSize));
  }
  /* Write Data in Flash - size has to be 64-bit aligned */
  ret = FLASH_SECONDARY_DEV_NAME.ProgramData(uFlashDestination, pData, uSize);
  if (ret != ARM_DRIVER_OK)
  {
    /*Reset of the ymodem variables */
    m_uFileSizeYmodem = 0U;
    m_uPacketsReceived = 0U;
    m_uNbrBlocksYmodem = 0U;
    return HAL_ERROR;
  }
  else
    return HAL_OK;
}

#else

void FW_UPDATE_Run(void)
{
  printf("Firmware update is unavailable without a secondary application slot.\r\n");
}

#endif /* (FLASH_SECONDARY_APP_SLOT_SIZE > 0U) || (defined MCUBOOT_PRIMARY_ONLY) */
