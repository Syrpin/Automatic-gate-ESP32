#include "init.h"

void Ivalue (void)
{
  if (IMAX < ISUM)
  {
    IMAX = ISUM;
  }
  INOM = IMAX;
}

void init (void)
{
  while (!(GPIOB->IDR & (BUTTON_OPEN_Pin | BUTTON_CLOSE_Pin)));
  
  HAL_TIM_Base_Start_IT(&htim1);

  IOFF = 0;
  INOM = 0;

  /*
  ******************************************************************************
    Choose Beep #2 and reset counter
  ******************************************************************************
  */
  beepVariant = 2;  
  beepClock = 0;
  
  /*
  ******************************************************************************

  ******************************************************************************
  */
  HAL_GPIO_WritePin (LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);

  /*
  ******************************************************************************
    Log - LOG_START_OR_REBOOT
  ******************************************************************************
  */
  
  /*
  ******************************************************************************
    Remember time. SavLog - LOG_START_OR_REBOOT_N_SEC
  ******************************************************************************
  */

  HAL_Delay (500);
  
  HAL_RTCEx_SetSecond_IT(&hrtc);
  while (Secunda < CURRENT_OFF_DURING_N_SEC)
  {
    if ( (GPIOB->IDR & ( MOTOR_OPEN_Pin | MOTOR_CLOSE_Pin)) == ( MOTOR_OPEN_Pin | MOTOR_CLOSE_Pin))
    {
      HAL_Delay (1);
      if (IOFF < ISUM)
      {
        IOFF = ISUM;
        INOM = ISUM;
      }
    }      
  }
  
  CONDITION |= ERROR_BYTE;

  /*
  ******************************************************************************
    Alarm LED - off
  ******************************************************************************
  */
  HAL_GPIO_WritePin (LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);

  /*
  ******************************************************************************
    Set encoder to Max position
  ******************************************************************************
  */
  TIM3->CNT = encMAX;

  /*
  ******************************************************************************
    Enable motor (fuse)
  ******************************************************************************
  */
  HAL_GPIO_WritePin(GPIOB, MOTOR_ENABLE_Pin, GPIO_PIN_SET);

  /*
  ******************************************************************************
    Wait untill full close and calculate INOM (IMAX) when Motor close.
    If encoder count towards, switch polarity (from RISING to FALLING).
  ******************************************************************************
  */

  while (GPIOB->IDR & SWITCH_CLOSE_Pin)
  {
    if (TIM3->CNT > 65000)
    {
      TIM3->CCER |= 2;
    }
    Ivalue();
  }

  /*
  ******************************************************************************
    Wait when gate stop (inertions)
  ******************************************************************************
  */
  HAL_Delay (1000);

  /*
  ******************************************************************************
    Set MIN encoder position
  ******************************************************************************
  */
  encMIN = 0;

  /*
  ******************************************************************************
    Blink when Ready
  ************************************************************************
  */
  HAL_GPIO_WritePin (LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
  HAL_Delay (500);
  HAL_GPIO_WritePin (LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);

  /*
  ******************************************************************************
    Set encoder to MIN position
  ******************************************************************************
  */
  TIM3->CNT = encMIN;

  /*
  ******************************************************************************
    Wait untill full open and calculate INOM (IMAX) when Motor open.
  ******************************************************************************
  */
  while (GPIOB->IDR & SWITCH_OPEN_Pin)
  {
    Ivalue () ;
  }

  /*
  ******************************************************************************
    Save INOM and IMAX
  ******************************************************************************
  */
  
  /*
  ******************************************************************************
    If no-encoder set working parameters
  ******************************************************************************
  */
  int a = 100;
  if (TIM3->CNT == encMIN)
  {
    encMAX = 2;
    TIM3->CNT = 1;
  }
  else
  {
    encMAX = TIM3->CNT;
    a = 0;
  }

  /*
  ******************************************************************************
    Calculate Distance and 1/3 and 2/3 position. Log LOG_FULL_DISTANCE
  ******************************************************************************
  */
  distance = encMAX - (encMIN);
  uint16_t distance1div3 = (distance / 3) ;
  uint16_t distance2div3 = (distance1div3 * 2) ;
  
  /*
  ******************************************************************************
    If encoder - Close to 1/3 and Open to 2/3.
    Calculate gate inertion.
  ******************************************************************************
  */
  if (a != 100)
  {
    HAL_Delay (5000) ;
    gateClose () ;
    while (TIM3->CNT > distance1div3);

    gateStop ();
    HAL_Delay (4000) ;
    int lag1 = distance1div3 - TIM3->CNT;
    encMIN += lag1;
  }
  if (a != 100)
  {
    HAL_Delay (1000) ;
    gateOpen () ;
    while (TIM3->CNT < distance2div3);

    gateStop () ;
    HAL_Delay (4000) ;
    int lag2 = TIM3->CNT - (distance2div3);
    HAL_Delay (1000) ;
    encMAX -= lag2;
  }

  /*
  ******************************************************************************
    Choose Beep #2 and reset counter
  ******************************************************************************
  */
  while (!(GPIOB->IDR & ( MOTOR_OPEN_Pin | MOTOR_CLOSE_Pin)));
  beepVariant = 2;
  beepClock = 0;
}
