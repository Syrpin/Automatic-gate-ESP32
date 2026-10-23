/* Includes ------------------------------------------------------------------*/
#include "brief.h"

const uint16_t CURRENT_BREAK = (CURRENT_BREAK_AFTER_N_SEC * BRIEF); //----------
uint16_t IMAXcount, IOFFcount;

void isError (uint8_t code)
{
  /*
  ******************************************************************************
    Stop
  ******************************************************************************
  */
  gateStop () ;

  /*
  ******************************************************************************
    Motor disable
  ******************************************************************************
  */
  HAL_GPIO_WritePin(GPIOB, MOTOR_ENABLE_Pin, GPIO_PIN_RESET);

  /*
  ******************************************************************************
    Set Beep variant, log error at first error
  ******************************************************************************
  */
  if  ((CONDITION & ERROR_BYTE) == ERROR_BYTE)
  {
    beepVariant = code;
  }

  /*
  ******************************************************************************
    Reset Error byte
  ******************************************************************************
  */
  CONDITION &= ~ERROR_BYTE;
}

void noError (void)
{
  /*
  ******************************************************************************
    Check Motor
  ******************************************************************************
  */
  if (! (GPIOB->IDR & ( MOTOR_OPEN_Pin | MOTOR_CLOSE_Pin)))
  {
    isError (LOG_ERROR_BOTH_DIRECT) ;
  }
  else
  {

    /*
    ******************************************************************************
      Check Switch
    ******************************************************************************
    */
    if (! (GPIOB->IDR & (SWITCH_OPEN_Pin | SWITCH_CLOSE_Pin)))
    {
      isError (LOG_ERROR_BOTH_SWITCH) ;
    }
    else
    {

      /*
      ******************************************************************************
        Check Current
      ******************************************************************************
      */
      if (ISUM > IOFF)
      {
        isError (LOG_ERROR_OFF_CURRENT) ;
      }
      else
      {


        /*
        ******************************************************************************
          Log Reset
        ******************************************************************************
        */

        /*
        ******************************************************************************
          Choose Beep #1 and reset counter
        ******************************************************************************
        */
        beepVariant = 1;
        //beepClock = 0;

        NVIC_SystemReset ();
      }
    }
  }
}

void iBreaker (void)
{
  /*
  ******************************************************************************
    Log Motor state
  ******************************************************************************
  */
  if  ((GPIOB->IDR & (MOTOR_OPEN_Pin | MOTOR_CLOSE_Pin)) != (MOTOR_OPEN_Pin | MOTOR_CLOSE_Pin))
  {
    if (ISUM > INOM)
    {
      IMAXcount++;
    }
    else
    {
      IMAXcount = 0;
    }
  }
  else {
    if (ISUM > IOFF)
    {
      IOFFcount++;
    }
    else
    {
      IOFFcount = 0;
    }
  }

  /*
  ******************************************************************************
    Log error
  ******************************************************************************
  */
  if (IMAXcount > CURRENT_BREAK)
  {
    isError(LOG_ERROR_ABOVE_NORMAL_CURRENT);
  }

  if (IOFFcount > CURRENT_BREAK)
  {
    isError(LOG_ERROR_OFF_CURRENT);
  }
}

void limitBrief (void)
{
  /*
  ******************************************************************************
    Check for error
  ******************************************************************************
  */
  if (! (GPIOB->IDR & (SWITCH_OPEN_Pin | SWITCH_CLOSE_Pin)))
  {
    isError (LOG_ERROR_BOTH_SWITCH);
  }

  /*
  ******************************************************************************
    Check open
  ******************************************************************************
  */
  if (! (GPIOB->IDR & SWITCH_OPEN_Pin))
  {
    CONDITION &= ~SWITCH_OPEN_BYTE;
  }
  else
  {
    CONDITION |= SWITCH_OPEN_BYTE;
  }

  /*
  ******************************************************************************
    Check close
  ******************************************************************************
  */
  if (! (GPIOB->IDR & SWITCH_CLOSE_Pin))
  {
    CONDITION &= ~SWITCH_CLOSE_BYTE;
  }
  else
  {
    CONDITION |= SWITCH_CLOSE_BYTE;
  }

  /*
  ******************************************************************************
    Check for stop
  ******************************************************************************
  */
  switch (CONDITION & PACK1)
  {
    case (ISCLOSE) : gateStop (); break;
    case (ISOPEN) : gateStop (); break;
  }
}

void motorBrief (void)
{

  /*
  ******************************************************************************
    Check for error
  ******************************************************************************
  */
  if (! (GPIOB->IDR & ( MOTOR_OPEN_Pin | MOTOR_CLOSE_Pin)))
  {
    isError (LOG_ERROR_BOTH_DIRECT);
  }

  /*
  ******************************************************************************
    Check for open
  ******************************************************************************
  */
  if (! (GPIOB->IDR & MOTOR_OPEN_Pin))
  {
    CONDITION &= ~MOTOR_OPEN_Pin_BYTE;
  }
  else
  {
    CONDITION |= MOTOR_OPEN_Pin_BYTE;
  }

  /*
  ******************************************************************************
    Check for close
  ******************************************************************************
  */
  if (! (GPIOB->IDR & MOTOR_CLOSE_Pin))
  {
    CONDITION &= ~MOTOR_CLOSE_Pin_BYTE;
  }
  else
  {
    CONDITION |= MOTOR_CLOSE_Pin_BYTE;
  }
}

void barrierBrief (void)
{
  /*
  ******************************************************************************
    Check barrier
  ******************************************************************************
  */
  if ((GPIOB->IDR & SWITCH_BARRIER_Pin))
  {
    CONDITION &= ~BARRIER_SWITCH_BYTE;

    /*
    ******************************************************************************
      Stop  if barrier
    ******************************************************************************
    */
    if ( (GPIOB->IDR & MOTOR_CLOSE_Pin) != (MOTOR_CLOSE_Pin))
    {
      gateStop ();  
    }
  }
  else
  {
    CONDITION |= BARRIER_SWITCH_BYTE;
  }
}

void encoderBrief (void)
{
  if (TIM3->CNT > encMIN)
  {
    CONDITION |= CLOSE_ENCODER_BYTE;
  }
  else {
    CONDITION &= ~CLOSE_ENCODER_BYTE;
  }
  if (TIM3->CNT < encMAX)
  {
    CONDITION |= OPEN_ENCODER_BYTE;
  }
  else {
    CONDITION &= ~OPEN_ENCODER_BYTE;
  }
  switch (CONDITION & PACK2)
  {
    case (COUNTCLOSE) : gateStop (); break;
    case (COUNTOPEN) : gateStop (); break;
  }
}
