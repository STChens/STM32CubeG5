/**
  ******************************************************************************
  * @file           : mx_pka.h
  * @brief          : Header for mx_pka.c file.
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
#ifndef MX_PKA_H
#define MX_PKA_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "stm32_hal.h"

hal_pka_handle_t *mx_pka_init(void);
void mx_pka_deinit(void);
hal_pka_handle_t *mx_pka_gethandle(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MX_PKA_H */
