/****************************************************************************
 Module
   LEDService.c

 Revision
   1.0.1

 Description
   This service lights LED based on color and blink when querying ball from dispenser

 Notes


****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
// Include Statements
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_Events.h"
#include "ES_Port.h"
#include "ES_Timers.h"

// the headers to access the GPIO subsystem
// for initializing the relevant TIVA pin
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_sysctl.h"

// the headers to access the TivaWare Library
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "BITDEFS.H"
#include "LEDService.h"


/*----------------------------- Module Defines ----------------------------*/
// define  ALL_BITS
#define ALL_BITS (0xff<<2)

// Assume a 1.000mS/tick timing
#define ONE_SEC 1000
#define BLINK_INTERVAL 100

#define ALL_BITS (0xff<<2)

#define RED 1
#define GREEN 0

#define TotalBlinkTimes 20

/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this service.They should be functions
   relevant to the behavior of this service
*/
static void TurnOnRedLED(void);
static void TurnOnGreenLED(void);
static void TurnOffBothLEDs(void);

/*---------------------------- Module Variables ---------------------------*/
// with the introduction of Gen2, we need a module level Priority variable
static uint8_t MyPriority;
static LEDState_t CurrentState;
static bool LED_ON;
static int BlinkTimes;


/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitLEDService

 Parameters
     uint8_t : the priorty of this service

 Returns
     bool, false if error in initialization, true otherwise

 Description
     Saves away the priority, and does any 
     other required initialization for this service
 Notes

****************************************************************************/
bool InitLEDService ( uint8_t Priority )
{
  MyPriority = Priority;
 // Initialize the port line PE0,PE1 for R & G LEDs
	HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R4;
	// Wait until clock is ready
	while ((HWREG(SYSCTL_PRGPIO) & SYSCTL_PRGPIO_R4) != SYSCTL_PRGPIO_R4);
	// Set bit 0&1 on Port E to be used as digital I/O lines
	HWREG(GPIO_PORTE_BASE+GPIO_O_DEN) |= GPIO_PIN_0;
	HWREG(GPIO_PORTE_BASE+GPIO_O_DEN) |= GPIO_PIN_1;
	// Set bit 0&1 on Port E to be output
	HWREG(GPIO_PORTE_BASE+GPIO_O_DIR) |= GPIO_PIN_0;
	HWREG(GPIO_PORTE_BASE+GPIO_O_DIR) |= GPIO_PIN_1;
	// Turn off both LEDs
	TurnOffBothLEDs();
	// Set CurrentState to Waiting4Command
	CurrentState = Waiting4Designation;
	// Set LED_ON to false
	LED_ON = false;
	// Set BlinkTimes to 0
	BlinkTimes = 0;
  ES_Event ThisEvent;
	ThisEvent.EventType = ES_INIT;
	if (ES_PostToService( MyPriority, ThisEvent) == true){
    return true;
	}
	else{
    return false;
	}
}

/****************************************************************************
 Function
     PostLEDService

 Parameters
     EF_Event ThisEvent ,the event to post to the queue

 Returns
     bool false if the Enqueue operation failed, true otherwise

 Description
     Posts an event to this state machine's queue
 Notes
****************************************************************************/
bool PostLEDService( ES_Event ThisEvent )
{
  return ES_PostToService( MyPriority, ThisEvent);
}


/****************************************************************************
 Function
    RunLEDService

 Parameters
   ES_Event : the event to process

 Returns
   ES_Event, ES_NO_EVENT if no error ES_ERROR otherwise

 Description

****************************************************************************/
ES_Event RunLEDService( ES_Event ThisEvent )
{
  ES_Event ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors
  printf("LED Service receives an event\r\n");
	// Based on the current state, choose the corresponding block of code
	switch(CurrentState){
	  // If it is in the beginning without knowing which color it is
		case Waiting4Designation:
			// If receiving an color designation event
		  if (ThisEvent.EventType  == ES_COLORDESIGNATION){
				printf("Now designate color\r\n");
				// Set LED_ON to true
				LED_ON = true;
			  // If parameter is RED
				if (ThisEvent.EventParam == RED){
				  // Set CurrentState to Red
					CurrentState = Red;
					// Turn on red LEDs
					TurnOnRedLED();
					printf("Red LED on\r\n");
				}
				// Else if parameter is GREEN
				if (ThisEvent.EventParam == GREEN){
				  // Set CurrentState to Green
					CurrentState = Green;
					// Turn on green LEDs
					TurnOnGreenLED();
					printf("Green LED on\r\n");
				}
			}
			break;
		// If it knows it is red
		case Red:
			// if this event is ES_QUERYBALL
		  if (ThisEvent.EventType == ES_QUERYBALL){
				// Initialize LED_BLINK_TIMER
			  ES_Timer_InitTimer(LED_BLINK_TIMER,BLINK_INTERVAL);
			}
			// if this event is LED_BLINK_TIMER timeout
			if (ThisEvent.EventType == ES_TIMEOUT && ThisEvent.EventParam == LED_BLINK_TIMER){
				// Switch on and off LED
				if (LED_ON){
				  TurnOffBothLEDs();
					LED_ON = false;
				}
				else {
				  TurnOnRedLED();
					LED_ON = true;
				}
				// Increment BlinkTimes
				BlinkTimes++;
				if (BlinkTimes < TotalBlinkTimes){
				  // Initialize LED_BLINK_TIMER again
				  ES_Timer_InitTimer(LED_BLINK_TIMER,BLINK_INTERVAL);
				}
				else {
					BlinkTimes = 0;
				}
			}
			
			break;
		// If it knows it is green
		case Green:
			// if this event is ES_QUERYBALL
		  if (ThisEvent.EventType == ES_QUERYBALL){
				// Initialize LED_BLINK_TIMER
			  ES_Timer_InitTimer(LED_BLINK_TIMER,BLINK_INTERVAL);
			}
			// if this event is LED_BLINK_TIMER timeout
			if (ThisEvent.EventType == ES_TIMEOUT && ThisEvent.EventParam == LED_BLINK_TIMER){
				// Switch on and off LED
				if (LED_ON){
				  TurnOffBothLEDs();
					LED_ON = false;
				}
				else {
				  TurnOnGreenLED();
					LED_ON = true;
				}
				// Increment BlinkTimes
				BlinkTimes++;
				if (BlinkTimes < TotalBlinkTimes){
				  // Initialize LED_BLINK_TIMER again
				  ES_Timer_InitTimer(LED_BLINK_TIMER,BLINK_INTERVAL);
				}
				else {
					BlinkTimes = 0;
				}
			}
			break;
	}
    
  return ReturnEvent;
}


static void TurnOnGreenLED(void){
  // Set PE0 to high
	HWREG(GPIO_PORTE_BASE+(GPIO_O_DATA+ALL_BITS)) |= (GPIO_PIN_0);
	// Set PE1 to low
	HWREG(GPIO_PORTE_BASE+(GPIO_O_DATA+ALL_BITS)) &= ~(GPIO_PIN_1);
}

static void TurnOnRedLED(void){
  // Set PE0 to low
	HWREG(GPIO_PORTE_BASE+(GPIO_O_DATA+ALL_BITS)) &= ~(GPIO_PIN_0);
	// Set PE1 to high
	HWREG(GPIO_PORTE_BASE+(GPIO_O_DATA+ALL_BITS)) |= (GPIO_PIN_1);
}

static void TurnOffBothLEDs(void){
  // Set PE0 to high
	HWREG(GPIO_PORTE_BASE+(GPIO_O_DATA+ALL_BITS)) |= (GPIO_PIN_0);
	// Set PE1 to high
	HWREG(GPIO_PORTE_BASE+(GPIO_O_DATA+ALL_BITS)) |= (GPIO_PIN_1);
}

