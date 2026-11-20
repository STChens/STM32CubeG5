/**
  ******************************************************************************
  * @file    low_level_rng.h
  * @brief   Application-side RNG interface used by MCUboot.
  ******************************************************************************
  */

#ifndef APPLI_LOW_LEVEL_RNG_H
#define APPLI_LOW_LEVEL_RNG_H

#include <stddef.h>
#include "mx_hal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported functions ------------------------------------------------------- */
void RNG_Init(void);
void RNG_GetBytes(uint8_t *output, size_t length, size_t *output_length);

#ifdef __cplusplus
}
#endif

#endif /* APPLI_LOW_LEVEL_RNG_H */
