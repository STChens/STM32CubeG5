/**
  ******************************************************************************
  * @file           : mx_hash.h
  * @brief          : Header for mx_hash.c file.
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
#ifndef MX_HASH_H
#define MX_HASH_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "stm32_hal.h"

hal_hash_handle_t *mx_hash_init(void);
void mx_hash_deinit(void);
hal_hash_handle_t *mx_hash_gethandle(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MX_HASH_H */
