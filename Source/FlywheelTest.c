/*----------------------------- Include Files -----------------------------*/
/* include header files for this state machine as well as any machines at the
   next lower level in the hierarchy that are sub-machines to this machine
*/
#include <stdio.h>

#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_Port.h"
#include "ES_DeferRecall.h"
#include "ES_Timers.h"
#include "termio.h"
#include "BITDEFS.h" // standard bit definitions to make things more readable

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

// headers from service files
#include "FlywheelTest.h"
#include "FlywheelPWM.h"

/*----------------------------- Module Defines ----------------------------*/
#define ALL_BITS (0xff<<2)

#define TicksPerMS 40000 //(25ns)/tick
#define SysClkFreq 40*1000000 //(40MHz)
#define FlyWEncoderTicksPerRev 64

#define TargetRPMVal 960

/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this service.They should be functions
   relevant to the behavior of this service
*/
static void Stop(void);
static void InitFlywheelInputCapture(void);
static void InitPIControlPeriodTimer(void);

/*---------------------------- Module Variables ---------------------------*/
// with the introduction of Gen2, we need a module level Priority variable
static uint8_t MyPriority;
static FlywheelTestState_t CurrentState;
// PI control parameters
static uint32_t Period;
static uint32_t LastCapture;
static uint16_t TargetRPM;
static uint16_t CurrentRPM;
static double RPMError;
static double SumError;
static int RequestedDuty; 
static double Kp = 0.2;
static double Ki = 0.01;
// for debug
static uint32_t debugger1;
static uint32_t debugger2;

/*------------------------------ Module Code ------------------------------*/
bool InitFlywheelTest ( uint8_t Priority )
{
  ES_Event ThisEvent;
  MyPriority = Priority;
	
	// Initialize PWM (PWMModule)
	InitFlywheelPWMModule();
	
	// Initialize interrupts (the control interrupt has NOT been enabled)
	InitFlywheelInputCapture();
	InitPIControlPeriodTimer();
	
	// State machine initialization
	LastCapture = 0;
	RequestedDuty = 0;
	CurrentState = IdleMode;
	Period = 100000000;
	puts("FlywheelTest Initializing...\r");
	
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

bool PostFlywheelTest( ES_Event ThisEvent )
{
  return ES_PostToService( MyPriority, ThisEvent);
}

ES_Event RunFlywheelTest( ES_Event ThisEvent )
{
  ES_Event ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors
  printf("FlywheelTest receives an event\r\n");
	if (ThisEvent.EventType == ES_RUNFLYWHEEL) {      // start running flywheel
		SetFlywheelDuty(30);
		TargetRPM = TargetRPMVal;
		CurrentRPM = 10;
		// enable the control interrupt
		HWREG(WTIMER3_BASE+TIMER_O_IMR) |= TIMER_IMR_TBTOIM; 					
		puts("Flywheel runs\r\n");
	}
	if (ThisEvent.EventType == ES_STOPFLYWHEEL) {      // for emergency stop
		// disable the control interrupt
		HWREG(WTIMER3_BASE+TIMER_O_IMR) &= ~TIMER_IMR_TBTOIM;
		CurrentRPM = 0;
		Stop();	
		puts("Flywheel stops\r\n");
	}
  
  return ReturnEvent;
}

void FlywheelInputCaptureISR(void)
{
	uint32_t ThisCapture;
	// start by clearing the source of the interrupt, the input capture event
	HWREG(WTIMER3_BASE+TIMER_O_ICR) = TIMER_ICR_CAECINT;
	debugger1++;
	// now grab the captured value and calculate the period
	ThisCapture = HWREG(WTIMER3_BASE+TIMER_O_TAR);
	Period = ThisCapture - LastCapture;
	// update LastCapture to prepare for the next edge
	LastCapture = ThisCapture;
	// enable the control interrupt (although a 2mS delay)
	HWREG(WTIMER3_BASE+TIMER_O_IMR) |= TIMER_IMR_TBTOIM;
}

void PIControlISR(void)
{
	// start by clearing the source of the interrupt
	HWREG(WTIMER3_BASE+TIMER_O_ICR) = TIMER_ICR_TBTOCINT;
	// increment our counter so that we can tell this interrupt is actually working
	debugger2++;
	if (Period == -1){SetFlywheelDuty(30);}
	else {
		CurrentRPM = (60.0*SysClkFreq)/(Period*FlyWEncoderTicksPerRev); // Calculate the current RPM based on Period
		if (CurrentRPM > 3000){CurrentRPM = 10;}
		RPMError = ((double)TargetRPM - (double)CurrentRPM);
		SumError += RPMError;
		RequestedDuty = (int)(Kp * (RPMError +  Ki* SumError));
		// add anti-windup for the integrator
		if (RequestedDuty > 100)  {
			RequestedDuty = 100;
			SumError -= RPMError;
		} else if (RequestedDuty < 0) {
			RequestedDuty = 0;
			SumError -= RPMError;
		}
		SetFlywheelDuty(RequestedDuty); // Update the PWM
	}
}

/***************************************************************************
 private functions
 ***************************************************************************/

static void Stop(void)
{
	SetFlywheelDuty(0);
	TargetRPM = 0;
}

static void InitFlywheelInputCapture(void)  // pin: PD2 (WT3CCP0)
{   
	// start by enabling the clock to the timer (Wide Timer 3)
	HWREG(SYSCTL_RCGCWTIMER) |= SYSCTL_RCGCWTIMER_R3;
	
	// enable the clock to Port D (PD2)
	HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R3;
	
	// make sure that timer (Timer A) is disabled before configuring
	HWREG(WTIMER3_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TAEN;
	
	// set it up in 32bit wide (individual, not concatenated) mode
	HWREG(WTIMER3_BASE+TIMER_O_CFG) = TIMER_CFG_16_BIT;
	
	// we want to use the full 32 bit count, so initialize the Interval Load
	// register to 0xffffffff
	HWREG(WTIMER3_BASE+TIMER_O_TAILR) = 0xffffffff;
	
	// set up timer A in capture mode (TAMR=3, TAAMS = 0),
	// for edge time (TACMR = 1) and up-counting (TACDIR = 1)
	HWREG(WTIMER3_BASE+TIMER_O_TAMR) =
	(HWREG(WTIMER3_BASE+TIMER_O_TAMR) & ~TIMER_TAMR_TAAMS) |
	(TIMER_TAMR_TACDIR | TIMER_TAMR_TACMR | TIMER_TAMR_TAMR_CAP);
	
	// To set the event to rising edge, we need to modify the TAEVENT bits
	// in GPTMCTL. Rising edge = 00, so we clear the TAEVENT bits
	HWREG(WTIMER3_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TAEVENT_M;
	
	// Now Set up the port to do the capture (clock was enabled earlier)
	// start by setting the alternate function for Port D bit 2 (WT3CCP0)
	HWREG(GPIO_PORTD_BASE+GPIO_O_AFSEL) |= BIT2HI;
	
	// Then, map bit 4's alternate function to WT3CCP0
	// 7 is the mux value to select WT3CCP0, 12 to shift it over to the
	// right nibble for bit 2 (4 bits/nibble * 2 bits)
	HWREG(GPIO_PORTD_BASE+GPIO_O_PCTL) =
	(HWREG(GPIO_PORTD_BASE+GPIO_O_PCTL) & 0xfffff0ff) + (7<<8);
	
	// Enable pin on Port D for digital I/O
	HWREG(GPIO_PORTD_BASE+GPIO_O_DEN) |= BIT2HI;
	// make pin 3 on Port D into an input
	HWREG(GPIO_PORTD_BASE+GPIO_O_DIR) &= BIT2LO;
	
	// back to the timer to enable a local capture interrupt
	HWREG(WTIMER3_BASE+TIMER_O_IMR) |= TIMER_IMR_CAEIM;
	
	// enable the Timer A in Wide Timer 3 interrupt in the NVIC
	// it is interrupt number 100 so appears in EN3 at bit 4
	HWREG(NVIC_EN3) |= BIT4HI;
	
	// make sure interrupts are enabled globally
	__enable_irq();
	
	// now kick the timer off by enabling it and enabling the timer to
	// stall while stopped by the debugger
	HWREG(WTIMER3_BASE+TIMER_O_CTL) |= (TIMER_CTL_TAEN | TIMER_CTL_TASTALL);
}

static void InitPIControlPeriodTimer(void) // pin: PD3 (WT3CCP1)
{
	// start by enabling the clock to the timer (Wide Timer 3)
	HWREG(SYSCTL_RCGCWTIMER) |= SYSCTL_RCGCWTIMER_R3;
	// kill a few cycles to let the clock get going
	while((HWREG(SYSCTL_PRWTIMER) & SYSCTL_PRWTIMER_R3) != SYSCTL_PRWTIMER_R3)
	{
	}
	
	// make sure that timer (Timer B) is disabled before configuring
	HWREG(WTIMER3_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TBEN; 
	
	// set it up in 32bit wide (individual, not concatenated) mode
	// the constant name derives from the 16/32 bit timer, but this is a 32/64
	// bit timer so we are setting the 32bit mode
	HWREG(WTIMER3_BASE+TIMER_O_CFG) = TIMER_CFG_16_BIT; //bits 0-2 = 0x04
	
	// set up timer B in periodic mode so that it repeats the time-outs
	HWREG(WTIMER3_BASE+TIMER_O_TBMR) =
	(HWREG(WTIMER3_BASE+TIMER_O_TBMR)& ~TIMER_TBMR_TBMR_M)|
	TIMER_TBMR_TBMR_PERIOD;
	
	// set timeout to be 2mS
	HWREG(WTIMER3_BASE+TIMER_O_TBILR) = TicksPerMS * 2;
	
	// enable a local timeout interrupt
	//HWREG(WTIMER3_BASE+TIMER_O_IMR) |= TIMER_IMR_TBTOIM; 
	
	// enable the Timer B in Wide Timer 3 interrupt in the NVIC
	// it is interrupt number 101 so appears in EN3 at bit 5
	HWREG(NVIC_EN3) |= BIT5HI;  
	
	// lower priority
	HWREG(NVIC_PRI25) = (HWREG(NVIC_PRI25) & ~NVIC_PRI25_INTD_M) + (1<<29);
	// make sure interrupts are enabled globally
	__enable_irq();
	
	// now kick the timer off by enabling it and enabling the timer to
	// stall while stopped by the debugger.
	HWREG(WTIMER3_BASE+TIMER_O_CTL) |= (TIMER_CTL_TBEN | TIMER_CTL_TBSTALL);
}

/*------------------------------- Footnotes -------------------------------*/
/*------------------------------ End of file ------------------------------*/

