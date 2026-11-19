/**
  ******************************************************************************
  * @file           : mx_saes.h
  * @brief          : Header for mx_saes.c file.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the mx_stm32g5xx_hal_drivers_license.md file
  * in the same directory as the generated code.
  * If no mx_stm32g5xx_hal_drivers_license.md file comes with the software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef MX_SAES_H
#define MX_SAES_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "stm32_hal.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

/******************************************************************************/
/* Exported functions for AES in HAL layer */
/******************************************************************************/
hal_aes_handle_t *mx_saes_init(void);
void mx_saes_deinit(void);
hal_aes_handle_t *mx_saes_gethandle(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MX_SAES_H */
