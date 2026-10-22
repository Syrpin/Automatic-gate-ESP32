#ifndef __brief_H
#define __brief_H
#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "define.h"
#include "motor.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void isError (uint8_t code);
void noError (void);
void iBreaker (void);
void motorBrief (void);
void limitBrief (void);
void barrierBrief (void);
void encoderBrief (void);
/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /*__ brief_H */
