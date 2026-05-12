#include "ky023.h"

static HAL_StatusTypeDef KY023_ReadChannel(ADC_HandleTypeDef *hadc,
                                           uint32_t channel,
                                           uint16_t *value);

void KY023_Init(KY023_HandleTypeDef *hky023,
                ADC_HandleTypeDef *hadc,
                GPIO_TypeDef *keyPort,
                uint16_t keyPin)
{
  if (hky023 == NULL)
  {
    Error_Handler();
  }

  hky023->hadc = hadc;
  hky023->keyPort = keyPort;
  hky023->keyPin = keyPin;
  hky023->stableState = 0U;
  hky023->candidateState = 0U;
  hky023->sameCount = 0U;
}

HAL_StatusTypeDef KY023_ReadRaw(KY023_HandleTypeDef *hky023,
                                uint16_t *xValue,
                                uint16_t *yValue)
{
  if ((hky023 == NULL) || (xValue == NULL) || (yValue == NULL))
  {
    return HAL_ERROR;
  }

  if (KY023_ReadChannel(hky023->hadc, ADC_CHANNEL_0, xValue) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (KY023_ReadChannel(hky023->hadc, ADC_CHANNEL_1, yValue) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

static HAL_StatusTypeDef KY023_ReadChannel(ADC_HandleTypeDef *hadc,
                                           uint32_t channel,
                                           uint16_t *value)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  if ((hadc == NULL) || (value == NULL))
  {
    return HAL_ERROR;
  }

  sConfig.Channel = channel;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;

  if (HAL_ADC_ConfigChannel(hadc, &sConfig) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (HAL_ADC_Start(hadc) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (HAL_ADC_PollForConversion(hadc, KY023_ADC_TIMEOUT_MS) != HAL_OK)
  {
    (void)HAL_ADC_Stop(hadc);
    return HAL_ERROR;
  }

  *value = (uint16_t)HAL_ADC_GetValue(hadc);

  if (HAL_ADC_Stop(hadc) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

uint8_t KY023_ReadKeyDebounced(KY023_HandleTypeDef *hky023)
{
  uint8_t rawState;

  if (hky023 == NULL)
  {
    Error_Handler();
  }

  rawState = (HAL_GPIO_ReadPin(hky023->keyPort, hky023->keyPin) == GPIO_PIN_RESET) ? 1U : 0U;

  if (rawState == hky023->candidateState)
  {
    if (hky023->sameCount < KY023_KEY_DEBOUNCE_COUNT)
    {
      hky023->sameCount++;
    }
    else
    {
      hky023->stableState = hky023->candidateState;
    }
  }
  else
  {
    hky023->candidateState = rawState;
    hky023->sameCount = 0U;
  }

  return hky023->stableState;
}

KY023_DirectionTypeDef KY023_GetDirection(uint16_t xValue, uint16_t yValue)
{
  int32_t xDelta;
  int32_t yDelta;
  int32_t absXDelta;
  int32_t absYDelta;

  xDelta = (int32_t)xValue - (int32_t)KY023_ADC_CENTER;
  yDelta = (int32_t)yValue - (int32_t)KY023_ADC_CENTER;
  absXDelta = (xDelta >= 0) ? xDelta : -xDelta;
  absYDelta = (yDelta >= 0) ? yDelta : -yDelta;

  if ((absXDelta <= (int32_t)KY023_ADC_DEADZONE) && (absYDelta <= (int32_t)KY023_ADC_DEADZONE))
  {
    return KY023_DIR_CENTER;
  }

  if (absXDelta > (absYDelta + (int32_t)KY023_ADC_AXIS_MARGIN))
  {
    if (xDelta < 0)
    {
      return KY023_DIR_RIGHT;
    }

    return KY023_DIR_LEFT;
  }

  if (absYDelta > (absXDelta + (int32_t)KY023_ADC_AXIS_MARGIN))
  {
    if (yDelta < 0)
    {
      return KY023_DIR_DOWN;
    }

    return KY023_DIR_UP;
  }

  if (absXDelta >= absYDelta)
  {
    if (xDelta < 0)
    {
      return KY023_DIR_RIGHT;
    }

    return KY023_DIR_LEFT;
  }

  if (yDelta < 0)
  {
    return KY023_DIR_DOWN;
  }

  return KY023_DIR_UP;
}

const char *KY023_GetDirectionString(KY023_DirectionTypeDef direction)
{
  switch (direction)
  {
    case KY023_DIR_UP:
      return "UP";
    case KY023_DIR_DOWN:
      return "DOWN";
    case KY023_DIR_LEFT:
      return "LEFT";
    case KY023_DIR_RIGHT:
      return "RIGHT";
    case KY023_DIR_UP_LEFT:
      return "UP_LEFT";
    case KY023_DIR_UP_RIGHT:
      return "UP_RIGHT";
    case KY023_DIR_DOWN_LEFT:
      return "DOWN_LEFT";
    case KY023_DIR_DOWN_RIGHT:
      return "DOWN_RIGHT";
    case KY023_DIR_CENTER:
    default:
      return "CENTER";
  }
}
