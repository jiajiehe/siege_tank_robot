/****************************************************************************
DCMotorService.c

This service controls the movement of the robot. Commands for location are
sent from the MasterSM. Public functions are:

void Drive(uint_8 Speed, uint_8 Direction);
void Stop(void);

The service uses a PI controller on velocity.

****************************************************************************/

/*----------------------------- Include Files -----------------------------*/
/* include header files for this state machine as well as any machines at the
   next lower level in the hierarchy that are sub-machines to this machine
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

// headers from service files
#include "DCMotorService.h"
#include "DCMotorPWM.h"
#include "ServoGateService.h"
#include "MasterSM.h"

/*----------------------------- Module Defines ----------------------------*/
#define ALL_BITS (0xff<<2)

// these times assume a 1.000mS/tick timing
#define ONE_SEC 1000
#define THREE_SEC 3*ONE_SEC
#define TEN_SEC (10*ONE_SEC)
#define TEN_MILLI_SEC (ONE_SEC/100)

// Identify motor direction
#define FWD 0
#define BWD 1

// Used for translating arrays
#define LMOTOR 0
#define RMOTOR 1

// Constants
#define TicksPerMS 40000 //(25ns)/tick
#define StallTimeout 50 //(mS)
#define SysClkFreq 40*1000000 //(40MHz)
#define GearRatio 50

#define PI 3.14159265358979323846
#define WheelDiameter 84
#define TicksPerRev 5 
#define ReportInterval 100
	
/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this service.They should be functions
   relevant to the behavior of this service
*/

// Initialization functions
static void InitLMotorInputCapture(void);
static void InitRMotorInputCapture(void);
static void InitVControlPeriodTimer(void);

// For PID control
static void VelocityControl(void);
static void ClearModuleVariables(void);

/*---------------------------- Module Variables ---------------------------*/
// with the introduction of Gen2, we need a module level Priority variable
static uint8_t MyPriority;
static DCMotorServiceState_t CurrentState;
static uint8_t MovingDirection;

// Velocity measurement variables
static uint32_t MotorPeriod[2];
static uint32_t LMotorLastCapture;
static uint32_t RMotorLastCapture;

static int16_t TargetRPM[2] = {0, 0};
static int16_t CurrentRPM[2] = {0, 0};
static double RPMError[2] = {0, 0};
static double SumRPMError[2] = {0, 0};
static double DutyCycle[2] = {0, 0};

// Velocity control function constants. Indexing = [LMOTOR, RMOTOR]
static double K_p_Velocity_FWD[2] = {1.5, 1.5};
static double K_i_Velocity_FWD[2]= {0, 0};

static double K_p_Velocity_BWD[2] = {1.5, 1.5};
static double K_i_Velocity_BWD[2]= {0, 0};

// Clamps.
static uint8_t DutyCycleClamp = 65;

/*------------------------------ Module Code ------------------------------*/
/*
InitDCMotorService

Initialize service
*/
bool InitDCMotorService ( uint8_t Priority )
{
  ES_Event ThisEvent;
  MyPriority = Priority;
	
	// Initialize PWM (PWMModule)
	InitDCMotorPWMModule();
	InitLMotorInputCapture();
	InitRMotorInputCapture();
	InitVControlPeriodTimer();
	// Clear module variables
  ClearModuleVariables();
	
	// Init Timer
	ES_Timer_Init(ES_Timer_RATE_1mS);
	CurrentState = Idle;
	
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

/*
PostDCMotorService

Post to service
*/
bool PostDCMotorService( ES_Event ThisEvent )
{
  return ES_PostToService( MyPriority, ThisEvent);
}

/*
RunDCMotorService

Run the service
*/

ES_Event RunDCMotorService( ES_Event ThisEvent )
{
  ES_Event ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors
	switch (CurrentState)
	{
		case Running:
				if (ThisEvent.EventType == ES_TIMEOUT && ThisEvent.EventParam == DC_STALL_TIMER) {
					Stop();
					ES_Event Event2Pass;
					Event2Pass.EventType = ES_MOTOR_STALL;
					PostMasterSM(Event2Pass);
				}
		break;
	} 
  return ReturnEvent;
}


/***************************************************************************
 public command functions
 ***************************************************************************/

/*
InitialDrive

Driving before arm expansion
*/
void InitialDrive(uint8_t Speed, uint8_t TargetDirection) {
	MovingDirection = TargetDirection;
	TargetRPM[RMOTOR] = 62;
	TargetRPM[LMOTOR] = 60;
	for (int i = 0; i < 2; i++) {
		SetDirection(i, MovingDirection);	
	}
	// Enable the VControl interrupt
	HWREG(WTIMER2_BASE+TIMER_O_IMR) |= TIMER_IMR_TATOIM;
	CurrentState = Running;
}

/*
Drive

Set the direction and target RPM
*/
void Drive(uint8_t Speed, uint8_t TargetDirection) {
	MovingDirection = TargetDirection;
	if(MovingDirection == FWD) {
		TargetRPM[RMOTOR] = 80;
		TargetRPM[LMOTOR] = 67;
	} else {
		TargetRPM[RMOTOR] = 80;
		TargetRPM[LMOTOR] = 63;	
	}
	for (int i = 0; i < 2; i++) {
		SetDirection(i, MovingDirection);	
	}
	// Enable the VControl interrupt
	HWREG(WTIMER2_BASE+TIMER_O_IMR) |= TIMER_IMR_TATOIM;
	CurrentState = Running;
}

void Stop(void) {
	// disable the VControl interrupt
	HWREG(WTIMER2_BASE+TIMER_O_IMR) &= ~TIMER_IMR_TATOIM;
	// set duty cycle to 0 to make sure motors stop
	SetDuty(LMOTOR, 0);
	SetDuty(RMOTOR, 0);
	// clear all module variables and 
	ClearModuleVariables();
	CurrentState = Idle;
}

/*
ControlISR

Interrupt for motor control loop.
*/
void ControlISR(void)
{ // start by clearing the source of the interrupt
	HWREG(WTIMER2_BASE+TIMER_O_ICR) = TIMER_ICR_TATOCINT;
	VelocityControl();
}

/*
LMotorInputCapture

Interrupt for LMotor Encoder Ticks. Updates tick count and calculated period.
*/
void LMotorInputCapture(void)
{ //pin: PC6 (WT1CCP0)
	uint32_t LMotorThisCapture;
	// start by clearing the source of the interrupt, the input capture event
	HWREG(WTIMER1_BASE+TIMER_O_ICR) = TIMER_ICR_CAECINT;
	// now grab the captured value and calculate the period
	LMotorThisCapture = HWREG(WTIMER1_BASE+TIMER_O_TAR);
	MotorPeriod[LMOTOR] = LMotorThisCapture - LMotorLastCapture;
	// update LastCapture to prepare for the next edge
	LMotorLastCapture = LMotorThisCapture;	
	// restart Distance OneShot Timer for determining whether LM has stopped
	ES_Timer_InitTimer(DC_STALL_TIMER, StallTimeout);
}

/*
RMotorInputCapture

Interrupt for RMotor Encoder Ticks. Updates tick count and calculated period.
*/
void RMotorInputCapture(void)
{ //pin: PC7 (WT1CCP1)
	uint32_t RMotorThisCapture;
	// start by clearing the source of the interrupt, the input capture event
	HWREG(WTIMER1_BASE+TIMER_O_ICR) = TIMER_ICR_CBECINT;
	// now grab the captured value and calculate the period
	RMotorThisCapture = HWREG(WTIMER1_BASE+TIMER_O_TBR);
	MotorPeriod[RMOTOR] = RMotorThisCapture - RMotorLastCapture;
	// update LastCapture to prepare for the next edge
	RMotorLastCapture = RMotorThisCapture;
	// restart Distance OneShot Timer for determining whether RM has stopped
	ES_Timer_InitTimer(DC_STALL_TIMER, StallTimeout);
}

/***************************************************************************
 private control loop functions
 ***************************************************************************/

/*
VelocityControl

Determines motor duty-cycle based on velocity error for translating
*/
static void VelocityControl(void) {
	for (int i = 0; i < 2; i++) {
		// Stop the robot and prevent oscillations	
		if(TargetRPM[i] == 0) {
			DutyCycle[i] = 0;
		} else {
			// Calculate current RPM
			CurrentRPM[i] = (60.0*SysClkFreq)/(GearRatio*MotorPeriod[i]*TicksPerRev);
			// If targetRPM is negative, set the currentRPM negative, and take the negative of the error
				RPMError[i] = ((double)TargetRPM[i] - (double)CurrentRPM[i]);
				SumRPMError[i] += RPMError[i];
			if(MovingDirection == FWD) {
				DutyCycle[i] = (int)(K_p_Velocity_FWD[i] * RPMError[i] +  K_i_Velocity_FWD[i]* SumRPMError[i]);
			} else {
				DutyCycle[i] = (int)(K_p_Velocity_BWD[i] * RPMError[i] +  K_i_Velocity_BWD[i]* SumRPMError[i]);
			}
			
			if (DutyCycle[i] > DutyCycleClamp)  {
				DutyCycle[i] = DutyCycleClamp;
				SumRPMError[i] -= RPMError[i];
			} else if (DutyCycle[i] < 0) {
				DutyCycle[i] = 0;
				SumRPMError[i] -= RPMError[i];
			}
		}
		SetDuty(i, DutyCycle[i]);
	}
}

/* ClearModuleVariables

Clears all driving related variables
*/
static void ClearModuleVariables(void){
	// Clear all module variables except Distance[ROBOT] and TargetDistance[ROBOT]
	for (int i = 0; i < 2; i++){
		RPMError[i] = 0;
		SumRPMError[i] = 0;
		CurrentRPM[i] = 0;
		TargetRPM[i] = 0;
  }
}

/***************************************************************************
 private initialization functions
 ***************************************************************************/

static void InitLMotorInputCapture(void)  //pin: PC6(WT1CCP0)
{
	// start by enabling the clock to the timer (Wide Timer 1)
	HWREG(SYSCTL_RCGCWTIMER) |= SYSCTL_RCGCWTIMER_R1;
	
	// enable the clock to Port C (PC6)
	HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R2;
	
	// make sure that timer (Timer A) is disabled before configuring
	HWREG(WTIMER1_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TAEN;
	
	// set it up in 32bit wide (individual, not concatenated) mode
	HWREG(WTIMER1_BASE+TIMER_O_CFG) = TIMER_CFG_16_BIT;
	
	// we want to use the full 32 bit count, so initialize the Interval Load
	// register to 0xffffffff
	HWREG(WTIMER1_BASE+TIMER_O_TAILR) = 0xffffffff;
	
	// set up timer A in capture mode (TAMR=3, TAAMS = 0),
	// for edge time (TACMR = 1) and up-counting (TACDIR = 1)
	HWREG(WTIMER1_BASE+TIMER_O_TAMR) =
	(HWREG(WTIMER1_BASE+TIMER_O_TAMR) & ~TIMER_TAMR_TAAMS) |
	(TIMER_TAMR_TACDIR | TIMER_TAMR_TACMR | TIMER_TAMR_TAMR_CAP);
	
	// To set the event to rising edge, we need to modify the TAEVENT bits
	// in GPTMCTL. Rising edge = 00, so we clear the TAEVENT bits
	HWREG(WTIMER1_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TAEVENT_M;
	
	// Now Set up the port to do the capture (clock was enabled earlier)
	// start by setting the alternate function for Port C bit 6 (WT1CCP0)
	HWREG(GPIO_PORTC_BASE+GPIO_O_AFSEL) |= BIT6HI;
	
	// Then, map bit 6's alternate function to WT1CCP0
	// 7 is the mux value to select WT1CCP0, 16 to shift it over to the
	// right nibble for bit 6 (4 bits/nibble * 6 bits)
	HWREG(GPIO_PORTC_BASE+GPIO_O_PCTL) =
	(HWREG(GPIO_PORTC_BASE+GPIO_O_PCTL) & 0xf0ffffff) + (7<<24);
	
	// Enable pin on Port C for digital I/O
	HWREG(GPIO_PORTC_BASE+GPIO_O_DEN) |= BIT6HI;
	// make pin 4 on Port C into an input
	HWREG(GPIO_PORTC_BASE+GPIO_O_DIR) &= BIT6LO;
	
	// back to the timer to enable a local capture interrupt
	HWREG(WTIMER1_BASE+TIMER_O_IMR) |= TIMER_IMR_CAEIM;
	
	// enable the Timer A in Wide Timer 1 interrupt in the NVIC
	// it is interrupt number 96 so appears in EN3 at bit 0
	HWREG(NVIC_EN3) |= BIT0HI;
	
	// make sure interrupts are enabled globally
	__enable_irq();
	
	// now kick the timer off by enabling it and enabling the timer to
	// stall while stopped by the debugger
	HWREG(WTIMER1_BASE+TIMER_O_CTL) |= (TIMER_CTL_TAEN | TIMER_CTL_TASTALL);
}

static void InitRMotorInputCapture(void) //pin: PC7(WT1CCP1)
{
	// start by enabling the clock to the timer (Wide Timer 1)
	HWREG(SYSCTL_RCGCWTIMER) |= SYSCTL_RCGCWTIMER_R1;
	
	// enable the clock to Port C (PC7)
	HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R2;
	
	// make sure that timer (Timer B) is disabled before configuring
	HWREG(WTIMER1_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TBEN;
	
	// set it up in 32bit wide (individual, not concatenated) mode
	HWREG(WTIMER1_BASE+TIMER_O_CFG) = TIMER_CFG_16_BIT;
	
	// we want to use the full 32 bit count, so initialize the Interval Load
	// register to 0xffffffff
	HWREG(WTIMER1_BASE+TIMER_O_TBILR) = 0xffffffff;
	
	// set up timer B in capture mode
	HWREG(WTIMER1_BASE+TIMER_O_TBMR) =
	(HWREG(WTIMER1_BASE+TIMER_O_TBMR) & ~TIMER_TBMR_TBAMS) |
	(TIMER_TBMR_TBCDIR | TIMER_TBMR_TBCMR | TIMER_TBMR_TBMR_CAP);
	
	// To set the event to rising edge, we need to modify the TBEVENT bits
	// in GPTMCTL. Rising edge = 00, so we clear the TBEVENT bits
	HWREG(WTIMER1_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TBEVENT_M;
	
	// Now Set up the port to do the capture (clock was enabled earlier)
	// start by setting the alternate function for Port C bit 7 (WT1CCP1)
	HWREG(GPIO_PORTC_BASE+GPIO_O_AFSEL) |= BIT7HI;
	
	// Then, map bit 7's alternate function to WT1CCP1
	// 7 is the mux value to select WT1CCP0, 16 to shift it over to the
	// right nibble for bit 7 (4 bits/nibble * 7 bits)
	HWREG(GPIO_PORTC_BASE+GPIO_O_PCTL) =
	(HWREG(GPIO_PORTC_BASE+GPIO_O_PCTL) & 0x0fffffff) + (7<<28);
	
	// Enable pin on Port C for digital I/O
	HWREG(GPIO_PORTC_BASE+GPIO_O_DEN) |= BIT7HI;
	// make pin 4 on Port C into an input
	HWREG(GPIO_PORTC_BASE+GPIO_O_DIR) &= BIT7LO;
	
	// back to the timer to enable a local capture interrupt
	HWREG(WTIMER1_BASE+TIMER_O_IMR) |= TIMER_IMR_CBEIM;
	
	// enable the Timer B in Wide Timer 1 interrupt in the NVIC
	// it is interrupt number 97 so appears in EN3 at bit 1
	HWREG(NVIC_EN3) |= BIT1HI;
	
	// make sure interrupts are enabled globally
	__enable_irq();
	
	// now kick the timer off by enabling it and enabling the timer to
	// stall while stopped by the debugger
	HWREG(WTIMER1_BASE+TIMER_O_CTL) |= (TIMER_CTL_TBEN | TIMER_CTL_TBSTALL);
}	

static void InitVControlPeriodTimer(void) //pin: PD0(WT2CCP0)
{
	// start by enabling the clock to the timer (Wide Timer 2)
	HWREG(SYSCTL_RCGCWTIMER) |= SYSCTL_RCGCWTIMER_R2;
	// kill a few cycles to let the clock get going
	while((HWREG(SYSCTL_PRWTIMER) & SYSCTL_PRWTIMER_R2) != SYSCTL_PRWTIMER_R2)
	{
	}
	
	// make sure that timer (Timer A) is disabled before configuring
	HWREG(WTIMER2_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TAEN; 
	
	// set it up in 32bit wide (individual, not concatenated) mode
	// the constant name derives from the 16/32 bit timer, but this is a 32/64
	// bit timer so we are setting the 32bit mode
	HWREG(WTIMER2_BASE+TIMER_O_CFG) = TIMER_CFG_16_BIT; //bits 0-2 = 0x04
	
	// set up timer A in periodic mode so that it repeats the time-outs
	HWREG(WTIMER2_BASE+TIMER_O_TAMR) =
	(HWREG(WTIMER2_BASE+TIMER_O_TAMR)& ~TIMER_TAMR_TAMR_M)|
	TIMER_TAMR_TAMR_PERIOD;
	
	// set timeout to be 2mS
	HWREG(WTIMER2_BASE+TIMER_O_TAILR) = TicksPerMS * 2;
	
	// enable a local timeout interrupt
	// This would be enabled every time when drive function is called
	// HWREG(WTIMER2_BASE+TIMER_O_IMR) |= TIMER_IMR_TATOIM; 
	
	// enable the Timer A in Wide Timer 2 interrupt in the NVIC
	// it is interrupt number 98 so appears in EN3 at bit 2
	HWREG(NVIC_EN3) |= BIT2HI; 
	
	// lower priority
	HWREG(NVIC_PRI24) = (HWREG(NVIC_PRI24) & ~NVIC_PRI24_INTC_M) + (1<<21);
	// make sure interrupts are enabled globally
	__enable_irq();
	
	// now kick the timer off by enabling it and enabling the timer to
	// stall while stopped by the debugger.
	HWREG(WTIMER2_BASE+TIMER_O_CTL) |= (TIMER_CTL_TAEN | TIMER_CTL_TASTALL);
}

/*------------------------------- Footnotes -------------------------------*/
/*------------------------------ End of file ------------------------------*/
