/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

extern uint8_t CONDITION, IOFF, encMIN;
extern uint8_t beepVariant, Secunda;
extern uint16_t closeTimer, encMAX, distance, beepClock;//, timerClock, briefClock;
extern const uint16_t CLOSETIMER;
extern uint32_t ISUM, INOM, IMAX; 
extern long int ISUM1, ISUM2;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BEEP_Pin GPIO_PIN_0
#define BEEP_GPIO_Port GPIOA
#define LED1_Pin GPIO_PIN_3
#define LED1_GPIO_Port GPIOA
#define CURRENT_TRANSFORMER_Pin GPIO_PIN_0
#define CURRENT_TRANSFORMER_GPIO_Port GPIOB
#define MOTOR_CLOSE_Pin GPIO_PIN_1
#define MOTOR_CLOSE_GPIO_Port GPIOB
#define MOTOR_OPEN_Pin GPIO_PIN_10
#define MOTOR_OPEN_GPIO_Port GPIOB
#define MOTOR_ENABLE_Pin GPIO_PIN_11
#define MOTOR_ENABLE_GPIO_Port GPIOB
#define SWITCH_BARRIER_Pin GPIO_PIN_15
#define SWITCH_BARRIER_GPIO_Port GPIOB
#define AC_PERIOD_Pin GPIO_PIN_15
#define AC_PERIOD_GPIO_Port GPIOA
#define AC_PERIOD_EXTI_IRQn EXTI15_10_IRQn
#define ENCODER_A_Pin GPIO_PIN_4
#define ENCODER_A_GPIO_Port GPIOB
#define ENCODER_B_Pin GPIO_PIN_5
#define ENCODER_B_GPIO_Port GPIOB
#define SWITCH_CLOSE_Pin GPIO_PIN_6
#define SWITCH_CLOSE_GPIO_Port GPIOB
#define SWITCH_OPEN_Pin GPIO_PIN_7
#define SWITCH_OPEN_GPIO_Port GPIOB
#define BUTTON_CLOSE_Pin GPIO_PIN_8
#define BUTTON_CLOSE_GPIO_Port GPIOB
#define BUTTON_OPEN_Pin GPIO_PIN_9
#define BUTTON_OPEN_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
