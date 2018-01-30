/***********************************************************************
COWSupplementService.c

This service will control IR emitter when the vehicle goes to the depot 
to supplement COWs. 

Rules: 1. A single COW each time when detector received a series of 10 pulses
10ms on time and 30ms off time. (signal frequency should be 25Hz)
2. Start a 3s timer after sending the series of pulses, during which time the vehicle
cannot request COWs anymore
***********************************************************************/
/*----------------------------- Include Files -----------------------------*/
/* include header files for this state machine as well as any machines at the
   next lower level in the hierarchy that are sub-machines to this machine
*/
// Include statements
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_Port.h"
#include "ES_DeferRecall.h"
#include "ES_Timers.h"
#include "termio.h"
#include "BITDEFS.h" // standard bit defintions to make things more readable

// the headers to access the GPIO subsystem
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_pwm.h"
#include "inc/hw_timer.h"
#include "inc/hw_nvic.h"

// the headers to access the TivaWare Library
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"

// the headers to other services
#include "MasterSM.h"
#include "COWSupplementService.h"
#include "LEDService.h"

/*----------------------------- Module Defines ----------------------------*/
#define ALL_BITS (0xff<<2)
// IR pulse parameters
#define TicksPerMS 40000
#define IRPulseONTime 10*TicksPerMS
#define IRPulseOFFTime 30*TicksPerMS
#define COWRequestInterval 3000

/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this machine.They should be functions
   relevant to the behavior of this state machine
*/
static void InitCOWPulse(void);

/*---------------------------- Module Variables ---------------------------*/
// everybody needs a state variable, you may need others as well.
// type of state variable should match htat of enum in header file
static COWSupplementState_t CurrentState;
static uint8_t MyPriority;
static uint8_t PulseCount;
static uint8_t RequestCount;

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitCOWSupplementService

 Parameters
     uint8_t : the priorty of this service

 Returns
     bool, false if error in initialization, true otherwise

 Description
     Saves away the priority, sets up the initial transition and does any
     other required initialization for this state machine
 Notes

 Author
     C. Zhang, 03/02/17, 15:01
****************************************************************************/
bool InitCOWSupplementService ( uint8_t Priority )
{
  ES_Event ThisEvent;
  MyPriority = Priority;
	// COW request IR emitter: PF4
	
	// Initialize the port line PF4 
	HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R5;
  while ((HWREG(SYSCTL_PRGPIO) & SYSCTL_PRGPIO_R5) != SYSCTL_PRGPIO_R5);
	// Set bit 4 on Port F to be used as digital I/O lines
	HWREG(GPIO_PORTF_BASE+GPIO_O_DEN) |= GPIO_PIN_4;
	// Set bit 4 on Port F to be output (emitter)
	HWREG(GPIO_PORTF_BASE+GPIO_O_DIR) |= GPIO_PIN_4;
	// Set PF4 to high at first
	HWREG(GPIO_PORTF_BASE+(GPIO_O_DATA+ALL_BITS)) |= GPIO_PIN_4;
	
	// Initialize timer for pulse
	InitCOWPulse();
	
	// Initialize framework timer resolution (1mS)
	ES_Timer_Init(ES_Timer_RATE_1mS);
	
	// Initialize all module vars
  CurrentState = Waiting4Reload;
	PulseCount = 0; 
	RequestCount = 0;
	
  // post the initial transition event
  ThisEvent.EventType = ES_INIT;
  if (ES_PostToService( MyPriority, ThisEvent) == true)
  {
		puts("Init COW\r\n");
      return true;
  }else
  {
      return false;
  }
}

/****************************************************************************
 Function
     PostCOWSupplementService

 Parameters
     EF_Event ThisEvent , the event to post to the queue

 Returns
     boolean False if the Enqueue operation failed, True otherwise

 Description
     Posts an event to this state machine's queue
 Notes

 Author
     C. Zhang, 03/02/17, 15:01
****************************************************************************/
bool PostCOWSupplementService( ES_Event ThisEvent )
{
  return ES_PostToService( MyPriority, ThisEvent);
}

/****************************************************************************
 Function
    RunCOWSupplementService

 Parameters
   ES_Event : the event to process

 Returns
   ES_Event, ES_NO_EVENT if no error ES_ERROR otherwise

 Description
   add your description here
 Notes
   uses nested switch/case to implement the machine.
 Author
   C. Zhang, 03/02/17, 15:01
****************************************************************************/
ES_Event RunCOWSupplementService( ES_Event ThisEvent )
{
  ES_Event ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors

  switch ( CurrentState )
  {
    case Waiting4Reload:       
		  if (ThisEvent.EventType == ES_RELOAD ){
				  puts("COW service starts pulsing!\r\n");
					ES_Event ThisEvent;
					ThisEvent.EventType = ES_QUERYBALL;
				  PostLEDService(ThisEvent);
				
					CurrentState = Waiting4FullLoad;
				  // set PF4 to low and start pulsing
				  HWREG(GPIO_PORTF_BASE+(GPIO_O_DATA+ALL_BITS)) &= ~GPIO_PIN_4;
				  // load IRPulseONTime to timer (10mS)
				  HWREG(WTIMER5_BASE+TIMER_O_TBILR) = IRPulseONTime;
					HWREG(WTIMER5_BASE+TIMER_O_TBV) = HWREG(WTIMER5_BASE+TIMER_O_TBILR);
				  // kick off timer to control ON time
				  HWREG(WTIMER5_BASE+TIMER_O_CTL) |= (TIMER_CTL_TBEN | TIMER_CTL_TBSTALL);
		  }
    break;
			
		case Waiting4FullLoad:      
				if (ThisEvent.EventType == ES_PULSE_DONE) {
					RequestCount++;
					PulseCount = 0;
					// start the query interval timer
					ES_Timer_InitTimer(COW_INTERVAL_TIMER, COWRequestInterval);
				}
				
				if (ThisEvent.EventType == ES_TIMEOUT && ThisEvent.EventParam == COW_INTERVAL_TIMER) {
					if (RequestCount < 5) {
						ES_Event ThisEvent;
				  	ThisEvent.EventType = ES_QUERYBALL;
				    PostLEDService(ThisEvent);
						// set PF4 to low and start pulsing
						//HWREG(GPIO_PORTF_BASE+(GPIO_O_DATA+ALL_BITS)) &= ~GPIO_PIN_4;
						// load IRPulseONTime to timer (10mS)
						HWREG(WTIMER5_BASE+TIMER_O_TBILR) = IRPulseONTime;
						HWREG(WTIMER5_BASE+TIMER_O_TBV) = HWREG(WTIMER5_BASE+TIMER_O_TBILR);
						// kick off timer to control ON time
						HWREG(WTIMER5_BASE+TIMER_O_CTL) |= (TIMER_CTL_TBEN | TIMER_CTL_TBSTALL);
					} else {
						PulseCount = 0;
						RequestCount = 0;
						CurrentState = Waiting4Reload;
						
						// Post ES_FULL_LOAD event back to MasterSM
						ES_Event ThisEvent;
						ThisEvent.EventType = ES_FULL_LOAD;
						PostMasterSM(ThisEvent);
						puts("Full load!\r\n");
					}
				}
    break;
			
    default :
      ;
  } 
  return ReturnEvent;
}

void PulseISR(void){
	// start by clearing the source of the interrupt
	HWREG(WTIMER5_BASE+TIMER_O_ICR) = TIMER_ICR_TBTOCINT;
	
	if ( (HWREG(GPIO_PORTF_BASE+(GPIO_O_DATA+ALL_BITS)) & BIT4HI) == BIT4HI ) {
		// set GPIO to low
		HWREG(GPIO_PORTF_BASE+(GPIO_O_DATA+ALL_BITS)) &= ~GPIO_PIN_4;		
		// reload timer value to IRPulseONTime (10mS) and kick off the timer
		HWREG(WTIMER5_BASE+TIMER_O_TBILR) = IRPulseONTime;
		HWREG(WTIMER5_BASE+TIMER_O_TBV) = HWREG(WTIMER5_BASE+TIMER_O_TBILR);
		HWREG(WTIMER5_BASE+TIMER_O_CTL) |= (TIMER_CTL_TBEN | TIMER_CTL_TBSTALL);
	} else if ( (HWREG(GPIO_PORTF_BASE+(GPIO_O_DATA+ALL_BITS)) & BIT4HI) == 0 ) {
		 // set GPIO to high
		 HWREG(GPIO_PORTF_BASE+(GPIO_O_DATA+ALL_BITS)) |= GPIO_PIN_4;
		 PulseCount++;
		 if (PulseCount < 10) {
		// reload timer value to IRPulseOFFTime (30mS) and kick off the timer;
			 HWREG(WTIMER5_BASE+TIMER_O_TBILR) = IRPulseOFFTime;
			 HWREG(WTIMER5_BASE+TIMER_O_TBV) = HWREG(WTIMER5_BASE+TIMER_O_TBILR);
			 HWREG(WTIMER5_BASE+TIMER_O_CTL) |= (TIMER_CTL_TBEN | TIMER_CTL_TBSTALL);
		} else { 
			ES_Event ThisEvent;
			ThisEvent.EventType = ES_PULSE_DONE;
			PostCOWSupplementService(ThisEvent);
		}
	}
}

/***************************************************************************
 private functions
 ***************************************************************************/
static void InitCOWPulse(void) {  // pin: PD7 (WT5CCP1)
	// start by enabling the clock to the timer (Wide Timer 5)
	HWREG(SYSCTL_RCGCWTIMER) |= SYSCTL_RCGCWTIMER_R5;
	// kill a few cycles to let the clock get going
	while((HWREG(SYSCTL_PRWTIMER) & SYSCTL_PRWTIMER_R5) != SYSCTL_PRWTIMER_R5)
	{
	}
	// make sure that timer (Timer B) is disabled before configuring
	HWREG(WTIMER5_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TBEN; //TBEN = Bit0
	// set it up in 32bit wide (individual, not concatenated) mode
	// the constant name derives from the 16/32 bit timer, but this is a 32/64
	// bit timer so we are setting the 32bit mode
	HWREG(WTIMER5_BASE+TIMER_O_CFG) = TIMER_CFG_16_BIT; //bits 0-2 = 0x04
	// set up timer B in 1-shot mode so that it disables timer on timeouts
	// first mask off the TAMR field (bits 0:1) then set the value for
	// 1-shot mode = 0x01
	HWREG(WTIMER5_BASE+TIMER_O_TBMR) =
	(HWREG(WTIMER5_BASE+TIMER_O_TBMR)& ~TIMER_TBMR_TBMR_M)|
	TIMER_TBMR_TBMR_1_SHOT;

	// set up timer B DOWN-counting (TBCDIR = 0)
	// so that rewrting the OneShotTimeout will restart timer B
	HWREG(WTIMER5_BASE+TIMER_O_TBMR) &= ~TIMER_TBMR_TBCDIR;

	// set timeout
	HWREG(WTIMER5_BASE+TIMER_O_TBILR) = IRPulseONTime;
	// enable a local timeout interrupt. TBTOIM = bit 0
	HWREG(WTIMER5_BASE+TIMER_O_IMR) |= TIMER_IMR_TBTOIM; // bit0
	// enable the Timer B in Wide Timer 5 interrupt in the NVIC
	// it is interrupt number 105 so appears in EN3 at bit 9
	HWREG(NVIC_EN3) |= BIT9HI;
	// make sure interrupts are enabled globally
	__enable_irq();

	// stop timer from starting for now, then start it when ES_RELOAD is posted
	//HWREG(WTIMER5_BASE+TIMER_O_CTL) |= (TIMER_CTL_TBEN | TIMER_CTL_TBSTALL);
}
