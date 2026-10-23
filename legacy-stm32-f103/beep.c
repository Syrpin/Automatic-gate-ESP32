#include "beep.h"

void beep (void)
{
  switch (beepVariant)
  {
    case 1: switch (beepClock)
      {
        case (1): TIM2->CCR1  =  VOLUME; break;
        case (500): TIM2->CCR1  =  0; break;
        case (600): TIM2->CCR1  =  VOLUME; break;
        case (1100): TIM2->CCR1  =  0; break;
        case (1200): beepClock = 0; break;
        default: break;
      } break;
    case 2: switch (beepClock)
      {
        case 1: TIM2->CCR1  =  VOLUME; break;
        case 4000: TIM2->CCR1  =  0; break;
        case 5000: beepVariant = 0; break;
        default: break;
      } break;

    case LOG_ERROR_OFF_CURRENT: switch (beepClock)
      {
        case 1: TIM2->CCR1  =  VOLUME; break;
        case 700: TIM2->CCR1  =  0; break;
        
        case 1000: TIM2->CCR1  =  VOLUME; break;
        case 1700: TIM2->CCR1  =  0; break;
        
        case 2000: TIM2->CCR1  =  VOLUME; break;
        case 2700: TIM2->CCR1  =  0; break;
        
        case 3700: beepClock = 0; break;
        default: break;
      } break;

    case LOG_ERROR_BOTH_DIRECT: switch (beepClock)
      {
        case 1: TIM2->CCR1  =  VOLUME; break;
        case 700: TIM2->CCR1  =  0; break;
        
        case 1000: TIM2->CCR1  =  VOLUME; break;
        case 1700: TIM2->CCR1  =  0; break;
        
        case 2000: TIM2->CCR1  =  VOLUME; break;
        case 2700: TIM2->CCR1  =  0; break;
        case 3700: beepClock = 0; break;
        default: break;
      } break;
    case LOG_ERROR_BOTH_SWITCH: switch (beepClock)
      {
        case 1: TIM2->CCR1  =  VOLUME; break;
        case 700: TIM2->CCR1  =  0; break;        
        case 1700: TIM2->CCR1  =  VOLUME; break;
        case 2000: TIM2->CCR1  =  0; break;
        
        case 2700: TIM2->CCR1  =  VOLUME; break;
        case 3000: TIM2->CCR1  =  0; break;
        
        case 3700: TIM2->CCR1  =  VOLUME; break;
        case 4000: TIM2->CCR1  =  0; break;        
        case 4700: TIM2->CCR1  =  VOLUME; break;
        case 5000: TIM2->CCR1  =  0; break;
        
        case 6000: beepClock = 0; break;
        default: break;
      } break;

    case LOG_ERROR_ABOVE_NORMAL_CURRENT: switch (beepClock)
      {
        case 1: TIM2->CCR1  =  VOLUME; break;
        case 700: TIM2->CCR1  =  0; break;        
        case 1000: TIM2->CCR1  =  VOLUME; break;
        case 1700: TIM2->CCR1  =  0; break;
                 
        case 2000: TIM2->CCR1  =  VOLUME; break;
        case 2700: TIM2->CCR1  =  0; break;
        
        case 3000: TIM2->CCR1  =  VOLUME; break;
        case 3700: TIM2->CCR1  =  0; break;
        case 4700: beepClock = 0; break;
        default: break;
      } break;

    default : TIM2->CCR1  =  0; break;
  }
}
