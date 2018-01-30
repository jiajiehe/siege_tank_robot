/****************************************************************************
 Module
   UltrasonicTest.c

 Revision
   1.0.1

 Description
   Use WideTimer 0 (PC4) as input capture to capture the periods between two rising
	 edges of ultrasonic sensor after it is triggered
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
/* include header files for the framework and this service
*/
#include <stdio.h>
#include <math.h>

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

// the headers from this and other services
#include "UltrasonicTest.h"
#include "MasterSM.h"

/*----------------------------- Module Defines ----------------------------*/
// these times assume a 1.000mS/tick timing
#define ONE_SEC 1000
#define HALF_SEC (ONE_SEC/2)
#define TWO_SEC (ONE_SEC*2)
#define FIVE_SEC (ONE_SEC*5)
#define TEN_MILISEC (ONE_SEC/100)
#define TicksPeruS 40 //(25ns)/tick
#define TriggerInterval 150 // Use 1000ms just for testing. 30mS is long enough to wait for a max echo return pulse
#define ShutoffCaptureInterval 140 //After 900ms of triggering sensor, input capture will will shut off
// define  ALL_BITS
#define ALL_BITS (0xff<<2)

/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this service.They should be functions
   relevant to the behavior of this service
*/
static void InitUltrasonicCapture(void);
static void InitOneShotTrigger(void);

/*---------------------------- Module Variables ---------------------------*/
// with the introduction of Gen2, we need a module level Priority variable
static uint8_t MyPriority;
// add a deferral queue for up to 3 pending deferrals +1 to allow for ovehead
static ES_Event DeferralQueue[3+1];
// for distance measurement
static uint32_t Period;
static uint32_t LastCapture;
static uint32_t ThisCapture;
static double Distance;
// for debugging
static int debugger1 = 0;
static int debugger2 = 0;
// for distance calculation
static double Temperature = 20; //(degree C)
static double SonicSpeed;
static bool isFirstOneShotISR = true;
// for posting event to MastSM
static bool report;
//static UltrasonicState_t CurrentState;
/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitUltrasonicTest

 Parameters
     uint8_t : the priorty of this service

 Returns
     bool, false if error in initialization, true otherwise

 Description
     Saves away the priority, and does any 
     other required initialization for this service
 Notes

 Author
     J. He & C. Zhang, 02/24/17, 14:23
****************************************************************************/
bool InitUltrasonicTest ( uint8_t Priority )
{
  ES_Event ThisEvent;
  
  MyPriority = Priority;
	
	// Initialize the port line PC4 
	HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R2;
  // Wait until clock is ready
  while ((HWREG(SYSCTL_PRGPIO) & SYSCTL_PRGPIO_R2) != SYSCTL_PRGPIO_R2);
	// Set bit 4 on Port C to be used as digital I/O lines
	HWREG(GPIO_PORTC_BASE+GPIO_O_DEN) |= GPIO_PIN_4;
	// Set bit 4 on Port C to be output
	HWREG(GPIO_PORTC_BASE+GPIO_O_DIR) |= GPIO_PIN_4;
	
  // Initialize trigger pulse one-shot timer
	InitOneShotTrigger();
	// Input capture interrupt will be initialized in one-shot timer ISR
	
	// Initialize framework timer resolution (1mS)
	ES_Timer_Init(ES_Timer_RATE_1mS);
	ES_Timer_InitTimer(ULTRASONICTRIG_TIMER, TriggerInterval);
	// ES_Timer_InitTimer(ULTRASONICSHUTOFF_TIMER, ShutoffCaptureInterval);
	
	// calculate sonic speed based on temperature
	SonicSpeed = 331.5 + (0.6 * Temperature); //(m/s)
	
	// set Report to false
	report = false;
  // post the initial transition event
  ThisEvent.EventType = ES_INIT;
  if (ES_PostToService( MyPriority, ThisEvent) == true)
  {
      return true;
  }else
  {
      return false;
  }
}

/****************************************************************************
 Function
     PostUltrasonicTest

 Parameters
     EF_Event ThisEvent ,the event to post to the queue

 Returns
     bool false if the Enqueue operation failed, true otherwise

 Description
     Posts an event to this state machine's queue
 Notes

 Author
     J. He & C. Zhang, 02/24/17, 14:23
****************************************************************************/
bool PostUltrasonicTest( ES_Event ThisEvent )
{
  return ES_PostToService( MyPriority, ThisEvent);
}

/****************************************************************************
 Function
    RunUltrasonicTest

 Parameters
   ES_Event : the event to process

 Returns
   ES_Event, ES_NO_EVENT if no error ES_ERROR otherwise

 Description
   add your description here
 Notes
   
 Author
   J. He & C. Zhang, 02/24/17, 14:23
****************************************************************************/
ES_Event RunUltrasonicTest( ES_Event ThisEvent )
{
  ES_Event ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors
    	// Based on type of event, choose different block of code
			switch (ThisEvent.EventType){
				
				// If ThisEvent is ES_START_ULTRASONIC
				case ES_START_ULTRASONIC:
					// set report to true
				  report = true;
				  printf("Start reporting Ultrasonic sensor\r\n");
					break;
				// If ThisEvent is ultrosonictimer time out
				case ES_TIMEOUT :
					if (ThisEvent.EventParam == ULTRASONICTRIG_TIMER){
						// set trigger GPIO (PC4) to high
						HWREG(GPIO_PORTC_BASE+(GPIO_O_DATA+ALL_BITS)) |= GPIO_PIN_4;
						// reload timeout to one-shot timer to restart it
						HWREG(WTIMER0_BASE+TIMER_O_TBV) = HWREG(WTIMER0_BASE+TIMER_O_TBILR);
						HWREG(WTIMER0_BASE+TIMER_O_CTL) |= (TIMER_CTL_TBEN | TIMER_CTL_TBSTALL);
						
					}
					else if (ThisEvent.EventParam == ULTRASONICSHUTOFF_TIMER){
						// locally disable ultrasonic input capture interrupt
						HWREG(WTIMER0_BASE+TIMER_O_IMR) &= ~TIMER_IMR_CAEIM;
						// make sure that timer (WideTimer0 Timer A) is disabled before doing any configuring to PC4
						HWREG(WTIMER0_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TAEN;
						// Now Set up the port back to GPIO port, by setting the alternate function for PC4 to low
						HWREG(GPIO_PORTC_BASE+GPIO_O_AFSEL) &= ~BIT4HI;
						
						// Initialize PC4 as GPIO digital output pin again
						
						// Initialize the port line PC4 
						HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R2;
						// Wait until clock is ready
						while ((HWREG(SYSCTL_PRGPIO) & SYSCTL_PRGPIO_R2) != SYSCTL_PRGPIO_R2);
						// Set bit 4 on Port C to be used as digital I/O lines
						HWREG(GPIO_PORTC_BASE+GPIO_O_DEN) |= GPIO_PIN_4;
						// Set bit 4 on Port C to be output
						HWREG(GPIO_PORTC_BASE+GPIO_O_DIR) |= GPIO_PIN_4;
					}
					break;
					
				// If ThisEvent is ultrasonic capture	
				case ES_ULTRASONIC_CAPTURE :
					Distance = ((double)Period * 25.0 / 1000000.0)* SonicSpeed / 2.0;
					// if reach the critical distance to the wall that corresponds to shooting area 2
					printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!Distance measured as: %f \r\n", Distance);		
				  if ((Distance < 900) && (Distance > 800) && (report == true)){
						// post event to MasterSM
						ES_Event Event2Post;
						Event2Post.EventType = ES_DISTANCE_DETECTED;
						PostMasterSM(Event2Post);
						// set report to false
						report = false;
						printf("Critical distance detected, measured as: %f \r\n", Distance);		
					}				
					break;
					
				default: break;
			}
  return ReturnEvent;
}

void UltrasonicCaptureResponse(void){
  uint8_t CurrentPortStatus;
	
	// start by clearing the source of the interrupt, the input capture event
	HWREG(WTIMER0_BASE+TIMER_O_ICR) = TIMER_ICR_CAECINT;
	debugger1++;
	// now grab the captured value
	ThisCapture = HWREG(WTIMER0_BASE+TIMER_O_TAR);
	// Update the current status of PC4
	CurrentPortStatus = (HWREG(GPIO_PORTC_BASE+(GPIO_O_DATA+ALL_BITS)) & GPIO_PIN_4);
	
	// If this is a rising edge
	if (CurrentPortStatus != 0){
	  // update LastCapture to prepare for the next edge
	  LastCapture = ThisCapture;
	}
	// Else if this is a falling edge
	else{
	  // Calculate period
	  Period = ThisCapture - LastCapture;
		// Post event with this period as parameter
		ES_Event Event2Post;
	  Event2Post.EventType = ES_ULTRASONIC_CAPTURE;
	  PostUltrasonicTest(Event2Post);
		// locally disable ultrasonic input capture interrupt
	  HWREG(WTIMER0_BASE+TIMER_O_IMR) &= ~TIMER_IMR_CAEIM;
	}
}

void OneShotTriggerISR(void)
{
	// start by clearing the source of the interrupt, the input capture event
	HWREG(WTIMER0_BASE+TIMER_O_ICR) = TIMER_ICR_TBTOCINT;
	// set trigger GPIO (PC4) to low
	HWREG(GPIO_PORTC_BASE+(GPIO_O_DATA+ALL_BITS)) &= ~GPIO_PIN_4;
	// set the timer for next measurement
	ES_Timer_InitTimer(ULTRASONICTRIG_TIMER, TriggerInterval);
	// set the timer for automatically shutting off input capture
	ES_Timer_InitTimer(ULTRASONICSHUTOFF_TIMER, ShutoffCaptureInterval);
	// Init PC4 as input capture with interrupt also enabled
	InitUltrasonicCapture();
	// Increment debugger2
	debugger2++;	
	}

/***************************************************************************
 private functions
 ***************************************************************************/
static void InitUltrasonicCapture(void){ // pin: PC4 (WT0CCP0)
  // start by enabling the clock to the timer(Wide Timer 0)
	HWREG(SYSCTL_RCGCWTIMER) |= SYSCTL_RCGCWTIMER_R0;
	
	// enable the clock to Port C
  HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R2;
  // since we added this Port C clock init, we can immediately start
  // into configuring the timer, no need for further delay

	// make sure that timer (Timer A) is disabled before configuring
  HWREG(WTIMER0_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TAEN;

	// set it up in 32bit wide (individual, not concatenated) mode
  // the constant name derives from the 16/32 bit timer, but this is a 32/64
  // bit timer so we are setting the 32bit mode
  HWREG(WTIMER0_BASE+TIMER_O_CFG) = TIMER_CFG_16_BIT;

	// we want to use the full 32 bit count, so initialize the Interval Load
  // register to 0xffff.ffff (its default value :-)
  HWREG(WTIMER0_BASE+TIMER_O_TAILR) = 0xffffffff;

	// set up timer A in capture mode (TAMR=3, TAAMS = 0),
  // for edge time (TACMR = 1) and up-counting (TACDIR = 1)
  HWREG(WTIMER0_BASE+TIMER_O_TAMR) =
    (HWREG(WTIMER0_BASE+TIMER_O_TAMR) & ~TIMER_TAMR_TAAMS) |
    (TIMER_TAMR_TACDIR | TIMER_TAMR_TACMR | TIMER_TAMR_TAMR_CAP);

  // To set the event to both edges, we need to modify the TAEVENT bits
  // in GPTMCTL. Both edge = 11, so we clear the TAEVENT bits
  HWREG(WTIMER0_BASE+TIMER_O_CTL) |= TIMER_CTL_TAEVENT_M;
	
	// Now Set up the port to do the capture (clock was enabled earlier)
  // start by setting the alternate function for Port C bit 4 (WT0CCP0)
  HWREG(GPIO_PORTC_BASE+GPIO_O_AFSEL) |= BIT4HI;

  // Then, map bit 4's alternate function to WT0CCP0
  // 7 is the mux value to select WT0CCP0, 16 to shift it over to the
  // right nibble for bit 4 (4 bits/nibble * 4 bits)
  HWREG(GPIO_PORTC_BASE+GPIO_O_PCTL) =
    (HWREG(GPIO_PORTC_BASE+GPIO_O_PCTL) & 0xfff0ffff) + (7<<16);

  // Enable pin on Port C for digital I/O
  HWREG(GPIO_PORTC_BASE+GPIO_O_DEN) |= BIT4HI;

  // make pin 4 on Port C into an input
  HWREG(GPIO_PORTC_BASE+GPIO_O_DIR) &= BIT4LO;

  // enable the local capture interrupt
	HWREG(WTIMER0_BASE+TIMER_O_IMR) |= TIMER_IMR_CAEIM;
	
	// disable the local capture interrupt for now, but the input capture interrupt will be enabled locally
	// in one shot timer ISR
	// HWREG(WTIMER0_BASE+TIMER_O_IMR) &= ~TIMER_IMR_CAEIM;

  // enable the Timer A in Wide Timer 0 interrupt in the NVIC
  // it is interrupt number 94 so appears in EN2 at bit 30
  HWREG(NVIC_EN2) |= BIT30HI;

  // make sure interrupts are enabled globally
  __enable_irq();

  // now kick the timer off by enabling it and enabling the timer to
  // stall while stopped by the debugger
  HWREG(WTIMER0_BASE+TIMER_O_CTL) |= (TIMER_CTL_TAEN | TIMER_CTL_TASTALL);
}

static void InitOneShotTrigger(void) // pin: PC5 (WT0CCP1)
{
	// start by enabling the clock to the timer (Wide Timer 0)
	HWREG(SYSCTL_RCGCWTIMER) |= SYSCTL_RCGCWTIMER_R0;
	// kill a few cycles to let the clock get going
	while((HWREG(SYSCTL_PRWTIMER) & SYSCTL_PRWTIMER_R0) != SYSCTL_PRWTIMER_R0)
	{
	}
	// make sure that timer (Timer B) is disabled before configuring
	HWREG(WTIMER0_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TBEN; //TBEN = Bit8
	// set it up in 32bit wide (individual, not concatenated) mode
	// the constant name derives from the 16/32 bit timer, but this is a 32/64
	// bit timer so we are setting the 32bit mode
	HWREG(WTIMER0_BASE+TIMER_O_CFG) = TIMER_CFG_16_BIT; //bits 0-2 = 0x04
	// set up timer B in 1-shot mode so that it disables timer on timeouts
	// first mask off the TBMR field (bits 0:1) then set the value for
	// 1-shot mode = 0x01
	HWREG(WTIMER0_BASE+TIMER_O_TBMR) =
	(HWREG(WTIMER0_BASE+TIMER_O_TBMR)& ~TIMER_TBMR_TBMR_M)|
	TIMER_TBMR_TBMR_1_SHOT;
	// set timeout
	HWREG(WTIMER0_BASE+TIMER_O_TBILR) = TicksPeruS * 10;    // datasheet: t(OUT) min 2uS, typical 5uS, whereas we choose 10 us here
	// enable a local timeout interrupt.
	HWREG(WTIMER0_BASE+TIMER_O_IMR) |= TIMER_IMR_TBTOIM; 
	// enable the Timer B in Wide Timer 0 interrupt in the NVIC
	// it is interrupt number 95 so appears in EN2 at bit 31
	HWREG(NVIC_EN2) |= BIT31HI;
	// make sure interrupts are enabled globally
	__enable_irq();
	
	// now kick the timer off by enabling it and enabling the timer to
	// stall while stopped by the debugger.
	//HWREG(WTIMER0_BASE+TIMER_O_CTL) |= (TIMER_CTL_TBEN | TIMER_CTL_TBSTALL);
}
/*------------------------------- Footnotes -------------------------------*/
/*------------------------------ End of file ------------------------------*/

