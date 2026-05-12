#ifndef __KY023_H
#define __KY023_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "adc.h"
#include "gpio.h"

#define KY023_ADC_TIMEOUT_MS      10U
#define KY023_KEY_DEBOUNCE_COUNT   3U
#define KY023_ADC_CENTER        2048U
#define KY023_ADC_DEADZONE        600U
#define KY023_ADC_AXIS_MARGIN      300U

typedef enum
{
  KY023_DIR_CENTER = 0,
  KY023_DIR_UP,
  KY023_DIR_DOWN,
  KY023_DIR_LEFT,
  KY023_DIR_RIGHT,
  KY023_DIR_UP_LEFT,
  KY023_DIR_UP_RIGHT,
  KY023_DIR_DOWN_LEFT,
  KY023_DIR_DOWN_RIGHT
} KY023_DirectionTypeDef;

typedef struct
{
  ADC_HandleTypeDef *hadc;
  GPIO_TypeDef *keyPort;
  uint16_t keyPin;
  uint8_t stableState;
  uint8_t candidateState;
  uint8_t sameCount;
} KY023_HandleTypeDef;

void KY023_Init(KY023_HandleTypeDef *hky023,
                ADC_HandleTypeDef *hadc,
                GPIO_TypeDef *keyPort,
                uint16_t keyPin);
HAL_StatusTypeDef KY023_ReadRaw(KY023_HandleTypeDef *hky023,
                                uint16_t *xValue,
                                uint16_t *yValue);
uint8_t KY023_ReadKeyDebounced(KY023_HandleTypeDef *hky023);
KY023_DirectionTypeDef KY023_GetDirection(uint16_t xValue, uint16_t yValue);
const char *KY023_GetDirectionString(KY023_DirectionTypeDef direction);

#ifdef __cplusplus
}
#endif

#endif /* __KY023_H */
