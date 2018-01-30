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

/*----------------------------- Module Defines ----------------------------*/
static void InitMotorCTL(void);
static void InitPWM(void);

/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this service.They should be functions
   relevant to the behavior of this service
*/
#define ALL_BITS (0xff<<2)
// 40,000 ticks per mS assumes a 40Mhz clock, we will use SysClk/32 for PWM
#define PWMTicksPerMS 40000/32
// set 1000Hz frequency so 1000uS period
#define PeriodInuS 1000

// program generator A to go to 1 at rising comare A, 0 on falling compare A  
#define GenA_Normal (PWM_0_GENA_ACTCMPAU_ONE | PWM_0_GENA_ACTCMPAD_ZERO )
// program generator B to go to 1 at rising comare B, 0 on falling compare B  
#define GenB_Normal (PWM_0_GENB_ACTCMPBU_ONE | PWM_0_GENB_ACTCMPBD_ZERO )

// for convenience to write data to registers
#define BitsPerNibble 4

// motor control
#define FWD 0
#define BWD 1
#define LMOTOR 0
#define RMOTOR 1

// motor PWM pins (drive-brake mode)
#define LMOTOR_CTL1 BIT0HI // PB0(LOW)
#define LMOTOR_CTL2 BIT6HI // PB6(PWM)
#define RMOTOR_CTL1 BIT1HI // PB1(LOW)
#define RMOTOR_CTL2 BIT7HI // PB7(PWM)

/*------------------------------ Module Code ------------------------------*/
void InitDCMotorPWMModule(void)
{
	InitMotorCTL();
	InitPWM();
}

void SetDirection(uint8_t Motor, uint8_t Direction)
{
	if (Motor == LMOTOR) {
		if (Direction == FWD) {
			HWREG(GPIO_PORTB_BASE + (GPIO_O_DATA+ALL_BITS)) &= ~(LMOTOR_CTL1);
			HWREG(PWM0_BASE + PWM_O_INVERT) &= ~(PWM_INVERT_PWM0INV);
		}
		
		if (Direction == BWD) {
			HWREG(GPIO_PORTB_BASE + (GPIO_O_DATA+ALL_BITS)) |= (LMOTOR_CTL1);
			HWREG(PWM0_BASE + PWM_O_INVERT) |= (PWM_INVERT_PWM0INV);
		}
	} else if (Motor == RMOTOR) {
			if (Direction == FWD) {
				HWREG(GPIO_PORTB_BASE + (GPIO_O_DATA+ALL_BITS)) &= ~(RMOTOR_CTL1);
				HWREG(PWM0_BASE + PWM_O_INVERT) &= ~(PWM_INVERT_PWM1INV);
			}
			
			if (Direction == BWD) {
				HWREG(GPIO_PORTB_BASE + (GPIO_O_DATA+ALL_BITS)) |= (RMOTOR_CTL1);
				HWREG(PWM0_BASE + PWM_O_INVERT) |= (PWM_INVERT_PWM1INV);
			}
	} else {
		puts("ERROR: Unable to set motor direction !!!\r\n");
	}
}

void SetDuty(uint8_t Motor, uint8_t Duty)
{
	if (Motor == LMOTOR) {
		if (Duty == 0) {
			HWREG( PWM0_BASE+PWM_O_0_GENA) = PWM_0_GENA_ACTZERO_ZERO;
		} else if (Duty == 100) {
			HWREG( PWM0_BASE+PWM_O_0_GENA) = PWM_0_GENA_ACTZERO_ONE;
		} else {
			HWREG( PWM0_BASE+PWM_O_0_GENA) = GenA_Normal;
			HWREG( PWM0_BASE+PWM_O_0_CMPA) = HWREG( PWM0_BASE+PWM_O_0_LOAD) - (HWREG( PWM0_BASE+PWM_O_0_LOAD)*Duty/100);		
		}
	} else if (Motor == RMOTOR) {
			if (Duty == 0) {
				HWREG( PWM0_BASE+PWM_O_0_GENB) = PWM_0_GENB_ACTZERO_ZERO;
			} else if (Duty == 100) {
				HWREG( PWM0_BASE+PWM_O_0_GENB) = PWM_0_GENB_ACTZERO_ONE;
			} else {
				HWREG( PWM0_BASE+PWM_O_0_GENB) = GenB_Normal;
				HWREG( PWM0_BASE+PWM_O_0_CMPB) = HWREG( PWM0_BASE+PWM_O_0_LOAD) - (HWREG( PWM0_BASE+PWM_O_0_LOAD)*Duty/100);		
			}
	} else {
		puts("ERROR: Unable to set motor speed !!!\r\n");
	}
}

/***************************************************************************
 private functions
 ***************************************************************************/
static void InitMotorCTL(void)
{
	HWREG(SYSCTL_RCGCGPIO)|= SYSCTL_RCGCGPIO_R1; // Port B
	while ((HWREG(SYSCTL_RCGCGPIO) & SYSCTL_RCGCGPIO_R1) != SYSCTL_RCGCGPIO_R1);
	// enable PB0, PB1 to be digital I/O and set them to be outputs
	HWREG(GPIO_PORTB_BASE+GPIO_O_DEN)|= (GPIO_PIN_0 | GPIO_PIN_1);
	HWREG(GPIO_PORTB_BASE+GPIO_O_DIR)|= (GPIO_PIN_0 | GPIO_PIN_1);
	// set PB0, PB1 to be initially LOW
	HWREG(GPIO_PORTB_BASE + (GPIO_O_DATA+ALL_BITS)) &= ~(LMOTOR_CTL1);
	HWREG(GPIO_PORTB_BASE + (GPIO_O_DATA+ALL_BITS)) &= ~(RMOTOR_CTL1);
}

static void InitPWM(void)
{
	// PB6(M0PWM0) & PB7(M0PWM1) are used to generate PWM (up-down counting)
	// start by enabling the clock to the PWM Module (PWM0)
  HWREG(SYSCTL_RCGCPWM) |= SYSCTL_RCGCPWM_R0;

	// enable the clock to Port B
  HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R1;

	// Select the PWM clock as System Clock/32
  HWREG(SYSCTL_RCC) = (HWREG(SYSCTL_RCC) & ~SYSCTL_RCC_PWMDIV_M) |
    (SYSCTL_RCC_USEPWMDIV | SYSCTL_RCC_PWMDIV_32);
  
	// make sure that the PWM module clock has gotten going
	while ((HWREG(SYSCTL_PRPWM) & SYSCTL_PRPWM_R0) != SYSCTL_PRPWM_R0)
    ;

	// disable the PWM while initializing
  HWREG( PWM0_BASE+PWM_O_0_CTL ) = 0; //(PWM Generator 0: PWM 0 & 1)

	// program generators to go to 1 at rising compare A/B, 0 on falling compare A/B  
	// GenA_Normal = (PWM_0_GENA_ACTCMPAU_ONE | PWM_0_GENA_ACTCMPAD_ZERO )
  HWREG( PWM0_BASE+PWM_O_0_GENA) = GenA_Normal;
	// GenB_Normal = (PWM_0_GENB_ACTCMPBU_ONE | PWM_0  _GENB_ACTCMPBD_ZERO )
  HWREG( PWM0_BASE+PWM_O_0_GENB) = GenB_Normal;

	// Set the PWM period. Since we are counting both up & down, we initialize
	// the load register to 1/2 the desired total period. We will also program
	// the match compare registers to 1/2 the desired high time  
  HWREG( PWM0_BASE+PWM_O_0_LOAD) = ((PeriodInuS * PWMTicksPerMS)/1000)>>1; // Period
	
	// Set the initial Duty cycle on A & B to 50% by programming the compare value
	// to 1/2 the period to count up (or down). Technically, the value to program
	// should be Period/2 - DesiredHighTime/2, but since the desired high time is 1/2 
	// the period, we can skip the subtract 
  HWREG( PWM0_BASE+PWM_O_0_CMPA) = HWREG( PWM0_BASE+PWM_O_0_LOAD)>>1; // Duty cycle
	HWREG( PWM0_BASE+PWM_O_0_CMPB) = HWREG( PWM0_BASE+PWM_O_0_LOAD)>>1;
	
	HWREG( PWM0_BASE+PWM_O_0_CMPA) = 0; // Duty cycle
	HWREG( PWM0_BASE+PWM_O_0_CMPB) = 0;
	
	// enable the PWM outputs
  HWREG( PWM0_BASE+PWM_O_ENABLE) |= (PWM_ENABLE_PWM0EN | PWM_ENABLE_PWM1EN);

	// now configure the Port B pins to be PWM outputs
	// start by selecting the alternate function for PB6 & 7
  HWREG(GPIO_PORTB_BASE+GPIO_O_AFSEL) |= (LMOTOR_CTL2 | RMOTOR_CTL2);

	// now choose to map PWM to those pins, this is a mux value of 4 that we
	// want to use for specifying the function on bits 6 & 7
  HWREG(GPIO_PORTB_BASE+GPIO_O_PCTL) = 
    (HWREG(GPIO_PORTB_BASE+GPIO_O_PCTL) & 0x00ffffff) + (4<<(6*BitsPerNibble)) +
      (4<<(7*BitsPerNibble));

	// Enable pins 6 & 7 on Port B for digital I/O
	HWREG(GPIO_PORTB_BASE+GPIO_O_DEN) |= (LMOTOR_CTL2 | RMOTOR_CTL2);
	
	// make pins 6 & 7 on Port B into outputs
	HWREG(GPIO_PORTB_BASE+GPIO_O_DIR) |= (LMOTOR_CTL2 | RMOTOR_CTL2);
  
	// set the up/down count mode, enable the PWM generator and make
	// both generator updates locally synchronized to zero count
  HWREG(PWM0_BASE+ PWM_O_0_CTL) = (PWM_0_CTL_MODE | PWM_0_CTL_ENABLE | 
                                    PWM_0_CTL_GENAUPD_LS | PWM_0_CTL_GENBUPD_LS);
}


/*------------------------------- Footnotes -------------------------------*/
/*------------------------------ End of file ------------------------------*/
