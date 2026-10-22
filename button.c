#include "button.h"

const uint16_t LONGPRESS = (LONGPRESS_AFTER_N_SEC * BRIEF + 10);
const uint8_t SHORTPRESS = (BRIEF / BRIEF + 5);

uint16_t btprev, debounce, motorprev;

void buttonBrief (void)
{
  if ( (GPIOB->IDR & (BUTTON_OPEN_Pin | BUTTON_CLOSE_Pin)) != (BUTTON_OPEN_Pin | BUTTON_CLOSE_Pin) )
  {
    uint16_t bt = GPIOB->IDR & (BUTTON_OPEN_Pin | BUTTON_CLOSE_Pin); ;
    if (bt == btprev)
    {
      debounce++;
      if (debounce >= LONGPRESS)
      {
        switch (btprev)
        {
          case BUTTON_CLOSE_Pin: if (debounce == LONGPRESS) 
            {
            
            }
            gateOpen () ; break;
          case BUTTON_OPEN_Pin: if (debounce == LONGPRESS) 
            {
            
            }
            gateClose () ; break;
          case ! (BUTTON_OPEN_Pin | BUTTON_CLOSE_Pin) : noError ()
            ; break;
          default: break;
        }
        debounce = (LONGPRESS + 1) ;
      }
      else
      {
        if (debounce > SHORTPRESS)
        {
          if (debounce == SHORTPRESS + 1)
          {
            motorprev = (GPIOB->IDR & (MOTOR_OPEN_Pin | MOTOR_CLOSE_Pin)) ;
          }
          else
          {
            if ( (GPIOB->IDR & (MOTOR_OPEN_Pin | MOTOR_CLOSE_Pin)) != (MOTOR_OPEN_Pin | MOTOR_CLOSE_Pin) )
              gateStop () ;
          }
        }
      }
    }
    else
    {
      btprev = bt;
    }
  }
  else
  {
    if (debounce > LONGPRESS)
    {
      gateStop () ;
    }
    else
    {
      if (debounce > SHORTPRESS)
      {
        if ( (motorprev) != (MOTOR_OPEN_Pin | MOTOR_CLOSE_Pin) )
        {
          motorprev = 0;
        }
        else
        {
          switch (CONDITION & (OPEN_ENCODER_BYTE | CLOSE_ENCODER_BYTE ) )
          {
            case OPEN_ENCODER_BYTE: gateOpen () ;
            break;
            case CLOSE_ENCODER_BYTE: gateClose () ;
            break;
            default:
              switch (GPIOB->IDR & (SWITCH_OPEN_Pin | SWITCH_CLOSE_Pin) )
              {
                case SWITCH_OPEN_Pin: gateOpen () ;
                break;
                case SWITCH_CLOSE_Pin: gateClose () ;
                break;
                case (SWITCH_OPEN_Pin | SWITCH_CLOSE_Pin) :
                  switch (btprev)
                  {
                    case BUTTON_CLOSE_Pin: gateOpen () ;
                    break;
                    case BUTTON_OPEN_Pin: gateClose () ;
                    break;
                    default: break;
                  }
                  break;
                default: break;
              }
              break;
          }
        }
      }
    }
    debounce = 0;
  }
}
