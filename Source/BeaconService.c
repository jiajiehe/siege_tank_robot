/***********************************************************************
BeaconService.c

This service measures the period of the IR input to the beacon service.
It uses the interrupt timer associated with tiva input PD7 (left IR sensor) and PD1 (right IR sensor)
***********************************************************************/

#include "BeaconService.h"

#define PeriodMargin 20  // in uS
#define CaptureMargin 10  // in uS
/*---------------------------- Module Variables ---------------------------*/
// with the introduction of Gen2, we need a module level Priority variable
static uint8_t BeaconPriority;

static const uint16_t PeriodArray[5] = {800, 690, 588, 513, 454}; // in uS
// static const uint16_t FreqArray[5] = {1250, 1450, 1700, 1950, 2200} // in Hz
static uint8_t Right_PeriodCount;
static uint16_t Right_TargetIRPeriod;
static uint32_t Right_LastCapture;
static uint32_t Right_LastPeriod;
static uint32_t Right_CapturedPeriod[32];
static bool Right_ReadytoSendPeriod;
static BeaconState_t BeaconState;

																	
/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this service.They should be functions
   relevant to the behavior of this service
*/
static void Right_InitBeaconCapture(void);

/*--------------------------Module Variables---------------------*/


/**************************
InitHallEffect

Initialize state machine
**************************/

bool InitBeaconService(uint8_t Priority) {
	BeaconPriority = Priority;
	Right_InitBeaconCapture(); 
	BeaconState = B_Waiting4Capture;
	puts("Init Beacon Service\r\n");
	ES_Event ThisEvent;
	ThisEvent.EventType = ES_INIT;
  if (ES_PostToService( BeaconPriority, ThisEvent) == true)
  {
      return true;
  }else
  {
      return false;
  }
}
/*
PostHallEffect

Posts events to service. Returns true if successful
*/

bool PostBeaconService(ES_Event ThisEvent) {
  return ES_PostToService(BeaconPriority, ThisEvent); 
}

/******************************************************************
RunBeaconService

This service determines the period of a magnetic field by checking
for 16 pulses with the same period. It updates the static variable
MagneticPeriod.
*******************************************************************/
ES_Event RunBeaconService(ES_Event ThisEvent) {
	ES_Event ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors
	
		switch(BeaconState) {
			case B_Waiting4Capture :
				if (ThisEvent.EventType == ES_INIT){
				puts("Beacon running\r\n");
				}
				if(ThisEvent.EventType == ES_START_IR_CAPTURE) {
					Right_TargetIRPeriod= PeriodArray[1]; //1450 Hz
					BeaconState = B_PeriodCapturing;
					// clear all module vars of the right sensor for a new-round capturing
					Right_LastCapture = 0;
					Right_LastPeriod = 0;
					Right_ReadytoSendPeriod = false;
					
					// Enable WTimer2B for right IR input capture
					HWREG(WTIMER2_BASE+TIMER_O_IMR) |= TIMER_IMR_CBEIM;
					printf("Start to capture IR\r\n");
				}
			break;

			case B_PeriodCapturing :
				// If the RIGHT sensor has averaged pulses ready
				if (ThisEvent.EventType == ES_IR_PULSE)
				{
					puts("Stable pulses received on the IR sensor\r\n");
					// disable input capture (already captured an averaged period, don't want to be interrupted any more)
					HWREG(WTIMER2_BASE+TIMER_O_IMR) &= ~TIMER_IMR_CBEIM;
					// Note that EventParam is 16 bit
						printf("Right_TargetIRPeriod = %d\r\n",Right_TargetIRPeriod);
						if (ThisEvent.EventParam >= (Right_TargetIRPeriod - CaptureMargin) &&
						ThisEvent.EventParam <= (Right_TargetIRPeriod + CaptureMargin))
						{
							printf("Valid IR period %d on the IR sensor\r\n",ThisEvent.EventParam);
							Right_ReadytoSendPeriod = true;
						}
				}
				if (Right_ReadytoSendPeriod)  // If the IR sensor captures valid pulses
				{
					// Post event to MasterSM
					ES_Event Event2Post;
					Event2Post.EventType = ES_BEACON_ALIGNED;
					Event2Post.EventParam = Right_TargetIRPeriod;
					PostMasterSM(Event2Post);
					puts("IR task complete. Event posted back to MasterSM\r\n");
				}
				// return to Waiting4Capture state regardless of whether (Left_ReadytoSendPeriod || Right_ReadytoSendPeriod) == true
				BeaconState = B_Waiting4Capture;
			break;
		}
	return ReturnEvent;
}

void RightIRCaptureISR(void) {
	uint32_t Right_ThisCapture = 0;
	uint32_t Right_ThisPeriod = 0;
// start by clearing the source of the interrupt, the input capture event (Wide timer 2B, PD1)
	HWREG(WTIMER2_BASE+TIMER_O_ICR) = TIMER_ICR_CBECINT;
// now grab the captured value and calculate the period
	Right_ThisCapture = HWREG(WTIMER2_BASE+TIMER_O_TBR); // ticks
	Right_ThisPeriod = (Right_ThisCapture - Right_LastCapture) * 25 / 1000; // convert ns to uS, tick interval = 25 ns
	Right_LastCapture = Right_ThisCapture;
	
	if (Right_PeriodCount == 0) {
		Right_CapturedPeriod[Right_PeriodCount] = Right_ThisPeriod;
		Right_PeriodCount++;
		Right_LastPeriod = Right_ThisPeriod;
	} else {
		
		if (Right_ThisPeriod > (Right_LastPeriod + CaptureMargin) || Right_ThisPeriod < (Right_LastPeriod - CaptureMargin)) {
			// clear all measured periods
			for (int i = 0; i < Right_PeriodCount; i++) {
				Right_CapturedPeriod[i] = 0;
			}
			// re-count from 0
			Right_PeriodCount = 0;
		} else {
			Right_CapturedPeriod[Right_PeriodCount] = Right_ThisPeriod;
			Right_LastPeriod = Right_ThisPeriod;
			Right_PeriodCount++;
			
		if (Right_PeriodCount == 32) {
			uint32_t Right_Sum = 0;
			for (int i = 0; i < Right_PeriodCount; i++) {
				Right_Sum += Right_CapturedPeriod[i];
			}
			Right_LastPeriod = Right_Sum >> 5; // faster way of calculating sum/32
			printf("Right average IR period = %d\r\n",Right_LastPeriod);
			ES_Event IRPeriodEvent;
			IRPeriodEvent.EventType = ES_IR_PULSE;
			IRPeriodEvent.EventParam = Right_LastPeriod;
			PostBeaconService(IRPeriodEvent);
		}
		}
	}
}

/*************************Private functions***********************/
/*
Right_InitBeaconCapture
This service initializes input capture function of WT2CCP1: PD1
*/
static void Right_InitBeaconCapture(void) { // pin PD1: WT2CCP1
	// start by enabling the clock to the timer (Wide Timer 2)
	HWREG(SYSCTL_RCGCWTIMER) |= SYSCTL_RCGCWTIMER_R2;

	// enable the clock to Port D
	HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R3;

	// since we added this Port D clock init, we can immediately start
	// into configuring the timer, no need for further delay
	// make sure that timer (Timer B) is disabled before configuring
	HWREG(WTIMER2_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TBEN;

	// set it up in 32bit wide (individual, not concatenated) mode
	// the constant name derives from the 16/32 bit timer, but this is a 32/64
	// bit timer so we are setting the 32bit mode
	HWREG(WTIMER2_BASE+TIMER_O_CFG) = TIMER_CFG_16_BIT;

	// we want to use the full 32 bit Count, so initialize the Interval Load
	// register to 0xffff.ffff (its default value :-)
	HWREG(WTIMER2_BASE+TIMER_O_TBILR) = 0xffffffff;

	// set up timer B in capture mode (TBMR=3, TBAMS = 0),
	// for edge time (TBCMR = 1) and up-Counting (TBCDIR = 1)
	HWREG(WTIMER2_BASE+TIMER_O_TBMR) = (HWREG(WTIMER2_BASE+TIMER_O_TBMR) & ~TIMER_TBMR_TBAMS) |
	(TIMER_TBMR_TBCDIR | TIMER_TBMR_TBCMR | TIMER_TBMR_TBMR_CAP);

	// To set the event to rising edge, we need to modify the TBEVENT bits
	// in GPTMCTL. Rising edge = 00, so we clear the TBEVENT bits
	HWREG(WTIMER2_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TBEVENT_M;

	// Now Set up the port to do the capture (clock was enabled earlier)
	// start by setting the alternate function for Port D bit 1 (WT2CCP1)
	HWREG(GPIO_PORTD_BASE+GPIO_O_AFSEL) |= BIT1HI;

	// Then, map bit 1's alternate function to WT2CCP1
	// 7 is the mux value to select WT2CCP1 (page 651), 4 to shift it over to the
	// right nibble for bit 1 (4 bits/nibble * 1 bit)
	HWREG(GPIO_PORTD_BASE+GPIO_O_PCTL) = (HWREG(GPIO_PORTD_BASE+GPIO_O_PCTL) & 0xffffff0f) + (7<<4);

	// Enable pin on Port D for digital I/O
	HWREG(GPIO_PORTD_BASE+GPIO_O_DEN) |= BIT1HI;

	// make pin 1 on Port D into an input
	HWREG(GPIO_PORTD_BASE+GPIO_O_DIR) &= BIT1LO;

	// locally disable capture interrupr for now, will be enabled later when MasterSM posts
	// ES_START_IR_CAPTURE event to this service
	HWREG(WTIMER2_BASE+TIMER_O_IMR) &= ~TIMER_IMR_CBEIM;

	// enable the Timer A in Wide Timer 5 interrupt in the NVIC
	// it is interrupt number 99 so appears in EN3 at bit 3
	HWREG(NVIC_EN3) |= BIT3HI;

	// make sure interrupts are enabled globally
  __enable_irq();

	// now kick the timer off by enabling it and enabling the timer to
	// stall while stopped by the debugger
	HWREG(WTIMER2_BASE+TIMER_O_CTL) |= (TIMER_CTL_TBEN | TIMER_CTL_TBSTALL);
}

