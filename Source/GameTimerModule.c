/***********************************************************************
GameTimerModule.c

One shot timer:  1st stop: 60 seconds, 2nd stop: 60 seconds, final stop: 20 seconds 
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
#define TWENTY_SECOND 20000*TicksPerMS
#define FREE_SHOOTING_WINDOW 20000*TicksPerMS

/*----------------------------- Module Variables ----------------------------*/
static uint8_t TimePassage = 0;

void InitGameTimer(void) {  // pin: PD4 (WT4CCP1)
	// start by enabling the clock to the timer (Wide Timer 4)
	HWREG(SYSCTL_RCGCWTIMER) |= SYSCTL_RCGCWTIMER_R4;
	// kill a few cycles to let the clock get going
	while((HWREG(SYSCTL_PRWTIMER) & SYSCTL_PRWTIMER_R4) != SYSCTL_PRWTIMER_R4)
	{
	}
	// make sure that timer (Timer B) is disabled before configuring
	HWREG(WTIMER4_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TBEN; //TBEN = Bit0
	// set it up in 32bit wide (individual, not concatenated) mode
	// the constant name derives from the 16/32 bit timer, but this is a 32/64
	// bit timer so we are setting the 32bit mode
	HWREG(WTIMER4_BASE+TIMER_O_CFG) = TIMER_CFG_16_BIT; //bits 0-2 = 0x04
	// set up timer B in 1-shot mode so that it disables timer on timeouts
	// first mask off the TAMR field (bits 0:1) then set the value for
	// 1-shot mode = 0x01
	HWREG(WTIMER4_BASE+TIMER_O_TBMR) =
	(HWREG(WTIMER4_BASE+TIMER_O_TBMR)& ~TIMER_TBMR_TBMR_M)|
	TIMER_TBMR_TBMR_1_SHOT;

	// set up timer B DOWN-counting (TBCDIR = 0)
	// so that rewrting the OneShotTimeout will restart timer B
	HWREG(WTIMER4_BASE+TIMER_O_TBMR) &= ~TIMER_TBMR_TBCDIR;

	// set timeout
	HWREG(WTIMER4_BASE+TIMER_O_TBILR) = TWENTY_SECOND;
	// enable a local timeout interrupt. TBTOIM = bit 0
	HWREG(WTIMER4_BASE+TIMER_O_IMR) |= TIMER_IMR_TBTOIM; // bit0
	// enable the Timer B in Wide Timer 4 interrupt in the NVIC
	// it is interrupt number 103 so appears in EN3 at bit 7
	HWREG(NVIC_EN3) |= BIT7HI;
	// make sure interrupts are enabled globally
	__enable_irq();

	// Kick off the game timer when the init function is called.
	HWREG(WTIMER4_BASE+TIMER_O_CTL) |= (TIMER_CTL_TBEN | TIMER_CTL_TBSTALL);
}

void GameTimerISR(void)
{
	// start by clearing the source of the interrupt
	HWREG(WTIMER4_BASE+TIMER_O_ICR) = TIMER_ICR_TBTOCINT;
	// load time to timer based on the TimePassage variable
	if (TimePassage < 5)
	{
		printf("Normal TimePassage\r\n");
		HWREG(WTIMER4_BASE+TIMER_O_TBILR) = TWENTY_SECOND;
		HWREG(WTIMER4_BASE+TIMER_O_TBV) = HWREG(WTIMER4_BASE+TIMER_O_TBILR);
		// kick off game timer again 
		HWREG(WTIMER4_BASE+TIMER_O_CTL) |= (TIMER_CTL_TBEN | TIMER_CTL_TBSTALL);
	}
	else if (TimePassage == 5)
	{
		printf("Last 20 sec TimePassage and Post Crazy Event to MasterSM\r\n");
		HWREG(WTIMER4_BASE+TIMER_O_TBILR) = FREE_SHOOTING_WINDOW;
		ES_Event Crazy;
		Crazy.EventType = ES_FREE_SHOOTING;
		PostMasterSM(Crazy);
		HWREG(WTIMER4_BASE+TIMER_O_TBV) = HWREG(WTIMER4_BASE+TIMER_O_TBILR);
		// kick off game timer again 
		HWREG(WTIMER4_BASE+TIMER_O_CTL) |= (TIMER_CTL_TBEN | TIMER_CTL_TBSTALL);
	}
	else if (TimePassage == 6)
	{
		ES_Event GameOver;
		GameOver.EventType = ES_GAME_OVER;
		PostMasterSM(GameOver);
	}
	TimePassage ++;
}

uint8_t QueryTimePassage(void)
{
	return TimePassage;
}
