/***********************************************************************
HallEffectService.c

This service measures the period of the input to the hall effect service.
It uses the interrupt timer associated with tiva input PD6
***********************************************************************/
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

#include "MasterSM.h"
#include "HallEffectService.h"
#include "DrivingSM.h"
#include "DCMotorPWM.h"
#include "DCMotorService.h"

/*---------------------------- Module # Defines ---------------------------*/
#define ALL_BITS (0xff<<2)
#define PeriodMargin 20

/*---------------------------- Module Variables ---------------------------*/
// with the introduction of Gen2, we need a module level Priority variable
static uint8_t HallEffectPriority;
static uint16_t PeriodArray[16] = {1333, 1277, 1222, 1166, 1111,
																	1055, 1000, 944, 889, 833,
																	778, 722, 667, 611, 556, 500};
static uint32_t ThisPeriod;
static uint8_t PeriodIndex;
static uint32_t LastCapture;
static uint32_t LastPeriod;
static uint8_t PeriodCount;
static uint32_t CapturedPeriod[32];
static bool ReadytoSendPeriod;
static uint8_t stageNum;
static int SensorStatus; // for future debugging
static HallEffectSensorState_t HallEffectState;
static int debugger;
   										
/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this service.They should be functions
   relevant to the behavior of this service
*/
static void InitHallEffectCapture(void);
static void InitSensors(uint8_t stageNum);
																	
/**************************
InitHallEffectService

Initialize state machine
**************************/

bool InitHallEffectService(uint8_t Priority) {
	HallEffectPriority = Priority;
	InitHallEffectCapture(); // input capture has not started yet (locally disable interrupt in Init function)
	HallEffectState = Waiting4Capture;
	
	// Initialize PB2 as GPIO pins for selecting mux
	HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R1;
  while ((HWREG(SYSCTL_PRGPIO) & SYSCTL_PRGPIO_R1) != SYSCTL_PRGPIO_R1);
	// Set bit 2 on Port B to be used as digital I/O lines
	HWREG(GPIO_PORTB_BASE+GPIO_O_DEN) |= GPIO_PIN_2;
	// Set bit 2 on Port B to be outputs
	HWREG(GPIO_PORTB_BASE+GPIO_O_DIR) |= GPIO_PIN_2;
	
	// Initialize framework timer resolution (1mS)
	ES_Timer_Init(ES_Timer_RATE_1mS);
	
	puts("Init Hall Effect\r\n");
	
	return true;
}
/*************************************************
PostHallEffectService

Posts events to service. Returns true if successful
**************************************************/

bool PostHallEffectService(ES_Event ThisEvent) {
  return ES_PostToService(HallEffectPriority, ThisEvent); 
}

/******************************************************************
RunHallEffectService

This service determines the period of a magnetic field by checking
for 16 pulses with the same period. It updates the static variable
MagneticPeriod.
*******************************************************************/
ES_Event RunHallEffectService(ES_Event ThisEvent) {
	ES_Event ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors
		switch(HallEffectState) {
			case Waiting4Capture :
				
				if(ThisEvent.EventType == ES_START_MAG_FIELD_CAPTURE) {
					HallEffectState = PeriodCapturing;
					// clear all module vars for a new-round capturing
					LastCapture = 0;
					PeriodCount = 0;
					LastPeriod = 0;
					ThisPeriod = 0;
					SensorStatus = 0; // a dummy status (real status is 1 or 2)
					for (int i = 0; i < 32; i++) {
						CapturedPeriod[i] = 0;
					}
					PeriodIndex = 0;
					ReadytoSendPeriod = false;
					
					// Choose to turn on which side of sensors based on stage number
					stageNum = ThisEvent.EventParam;  //!!! Change: stageNum is passed in the EventParam instead (LEFT or RIGHT)
					InitSensors(stageNum); // initialize sensors based on stage number
					// Enable WTimer5A for input capture
					HWREG(WTIMER5_BASE+TIMER_O_IMR) |= TIMER_IMR_CAEIM;
					printf("Start to capture mag field\r\n");
				}
				
				//if (ThisEvent.EventType == ES_NEW_KEY) {printf("debugger: %d\r\n", debugger);}
				if (ThisEvent.EventType == ES_NEW_KEY) {printf("This Period: %d\r\n", ThisPeriod);}
			break;

			case PeriodCapturing :
				if (ThisEvent.EventType == ES_MAGNETIC_PULSE) {
					puts("32 consistent puleses captured\r\n");
					// disable input capture (already captured a period, don't want to be interrupted any more)
					HWREG(WTIMER5_BASE+TIMER_O_IMR) &= ~TIMER_IMR_CAEIM;
					for (int i = 0; i < 16; i++) {
						if (ThisEvent.EventParam >= (PeriodArray[i] - 20) && ThisEvent.EventParam <= (PeriodArray[i] + 20)) {
							ReadytoSendPeriod = true;
							PeriodIndex = i;
						}
					}
					
					if (ReadytoSendPeriod) {
						// Post event to MasterSM
						ES_Event Event2Post;
						Event2Post.EventType = ES_MAG_FIELD;
						// Want to let Master know what period is captured so that it can print it out
						Event2Post.EventParam = PeriodArray[PeriodIndex];
						printf("Mag Freq: %duS\r\n", Event2Post.EventParam);
						PostMasterSM(Event2Post);
						puts("Mag found, posted back to Master\r\n");
						// return to Waiting4Capture state
						HallEffectState = Waiting4Capture;
						puts("Going back to Waiting4Capture......\r\n");
					} else {
						HWREG(WTIMER5_BASE+TIMER_O_IMR) |= TIMER_IMR_CAEIM;
					}
				}
				
				//if (ThisEvent.EventType == ES_NEW_KEY) {printf("debugger: %d\r\n", debugger);}
				if (ThisEvent.EventType == ES_NEW_KEY) {printf("This Period: %d\r\n", ThisPeriod);}
			break;
		}
	return ReturnEvent;
}

void HallEffectCaptureISR(void) {
	debugger++;
	uint32_t ThisCapture;
// start by clearing the source of the interrupt, the input capture event
	HWREG(WTIMER5_BASE+TIMER_O_ICR) = TIMER_ICR_CAECINT;
// now grab the captured value and calculate the period
	ThisCapture = HWREG(WTIMER5_BASE+TIMER_O_TAR); // ticks
	ThisPeriod = (ThisCapture - LastCapture) * 25 / 1000; // (uS)
	LastCapture = ThisCapture;
	
	if (PeriodCount == 0) {
		CapturedPeriod[PeriodCount] = ThisPeriod;
		PeriodCount++;
		LastPeriod = ThisPeriod;
	} else {
		
		if (ThisPeriod > (LastPeriod + 60) || ThisPeriod < (LastPeriod - 60)) {
			// clear all measured periods
			for (int i = 0; i < PeriodCount; i++) {
				CapturedPeriod[i] = 0;
			}
			// re-count from 0
			PeriodCount = 0;
		} else {
			CapturedPeriod[PeriodCount] = ThisPeriod;
			PeriodCount++;
			// update LastPeriod to be the average of all measured values
			uint32_t sum = 0;
			for (int i = 0; i < PeriodCount; i++) {
				sum += CapturedPeriod[i];
			}
			LastPeriod = sum/PeriodCount;
			
			if (PeriodCount == 32) {
				ES_Event MagPeriodEvent;
				MagPeriodEvent.EventType = ES_MAGNETIC_PULSE;
				MagPeriodEvent.EventParam = LastPeriod;
				PostHallEffectService(MagPeriodEvent);
			}
		}
	}
}

uint8_t getFreqCode(void) {
	// If ready to send period, return frequency code
	if(ReadytoSendPeriod) {
		puts("Frequency code sent to LOC\r\n");
		return PeriodIndex;
	}
	
	// If something wrong, return nonsense number
	return 255;
}
/***************************************************************************
 private functions
 ***************************************************************************/
static void InitSensors(uint8_t stageNum)
{
		if (stageNum == 1 || stageNum == 3) { // 1R or 3R (left)
			// set PB2 to be low 
			HWREG(GPIO_PORTB_BASE+(GPIO_O_DATA+ALL_BITS)) &= ~GPIO_PIN_2;
			SensorStatus = LEFT;
		} else if (stageNum == 2) { // 2R (right)
			// set PB2 to be high 
			HWREG(GPIO_PORTB_BASE+(GPIO_O_DATA+ALL_BITS)) |= GPIO_PIN_2;
			SensorStatus = RIGHT;
	  }
}

/*
InitHallEffectTimer
This service initializes input capture function of WT5CCP0: PD6
*/
static void InitHallEffectCapture(void) { // pin: PD6 (WT5CCP0)
	// start by enabling the clock to the timer (Wide Timer 5)
	HWREG(SYSCTL_RCGCWTIMER) |= SYSCTL_RCGCWTIMER_R5;

	// enable the clock to Port D
	HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R3;

	// since we added this Port D clock init, we can immediately start
	// into configuring the timer, no need for further delay
	// make sure that timer (Timer A) is disabled before configuring
	HWREG(WTIMER5_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TAEN;

	// set it up in 32bit wide (individual, not concatenated) mode
	// the constant name derives from the 16/32 bit timer, but this is a 32/64
	// bit timer so we are setting the 32bit mode
	HWREG(WTIMER5_BASE+TIMER_O_CFG) = TIMER_CFG_16_BIT;

	// we want to use the full 32 bit Count, so initialize the Interval Load
	// register to 0xffff.ffff (its default value :-)
	HWREG(WTIMER5_BASE+TIMER_O_TAILR) = 0xffffffff;

	// set up timer A in capture mode (TAMR=3, TAAMS = 0),
	// for edge time (TACMR = 1) and up-Counting (TACDIR = 1)
	HWREG(WTIMER5_BASE+TIMER_O_TAMR) = (HWREG(WTIMER5_BASE+TIMER_O_TAMR) & ~TIMER_TAMR_TAAMS) |
	(TIMER_TAMR_TACDIR | TIMER_TAMR_TACMR | TIMER_TAMR_TAMR_CAP);

	// To set the event to rising edge, we need to modify the TAEVENT bits
	// in GPTMCTL. Rising edge = 00, so we clear the TAEVENT bits
	HWREG(WTIMER5_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TAEVENT_M;

	// Now Set up the port to do the capture (clock was enabled earlier)
	// start by setting the alternate function for Port D bit 6 (WT5CCP0)
	HWREG(GPIO_PORTD_BASE+GPIO_O_AFSEL) |= BIT6HI;

	// Then, map bit 4's alternate function to WT5CCP0
	// 7 is the mux value to select WT5CCP0, 16 to shift it over to the
	// right nibble for bit 6 (4 bits/nibble * 6 bits)
	HWREG(GPIO_PORTD_BASE+GPIO_O_PCTL) = (HWREG(GPIO_PORTD_BASE+GPIO_O_PCTL) & 0xf0ffffff) + (7<<24);

	// Enable pin on Port D for digital I/O
	HWREG(GPIO_PORTD_BASE+GPIO_O_DEN) |= BIT6HI;

	// make pin 6 on Port D into an input
	HWREG(GPIO_PORTD_BASE+GPIO_O_DIR) &= BIT6LO;

	// locally disable capture interrupr for now, will be enabled later when MasterSM posts
	// ES_START_MAG_FIELD_CAPTURE event to this service
	HWREG(WTIMER5_BASE+TIMER_O_IMR) &= ~TIMER_IMR_CAEIM;

	// enable the Timer A in Wide Timer 5 interrupt in the NVIC
	// it is interrupt number 104 so appears in EN3 at bit 8
	HWREG(NVIC_EN3) |= BIT8HI;

	// make sure interrupts are enabled globally
  __enable_irq();

	// now kick the timer off by enabling it and enabling the timer to
	// stall while stopped by the debugger
	HWREG(WTIMER5_BASE+TIMER_O_CTL) |= (TIMER_CTL_TAEN | TIMER_CTL_TASTALL);
}

