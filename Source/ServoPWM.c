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
#define ALL_BITS (0xff<<2)
// 40,000 ticks per mS assumes a 40Mhz clock, we will use SysClk/32 for PWM
#define PWMTicksPerMS 40000/32
// set 200Hz frequency so 20000uS period
#define PeriodInuS 20000

// program generator A to go to 1 at rising compare A, 0 on falling compare A  
#define GenA_Normal (PWM_3_GENA_ACTCMPAU_ONE | PWM_3_GENA_ACTCMPAD_ZERO )
// program generator B to go to 1 at rising compare B, 0 on falling compare B  
#define GenB_Normal (PWM_3_GENB_ACTCMPBU_ONE | PWM_3_GENB_ACTCMPBD_ZERO )

// for convenience to write data to registers
#define BitsPerNibble 4

// for choosing servo motors
#define GATE_SERVO   0        //PF0
#define SENSOR_SERVO 1        //PF1
#define LEFT_ROLLER_SERVO 2   //PF2
#define RIGHT_ROLLER_SERVO 3  //PF3

/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this service.They should be functions
   relevant to the behavior of this service
*/


/*------------------------------ Module Code ------------------------------*/

void InitServoPWM(void)
{
	// PE5(M1PWM3) & PF1(M1PWM5) & PF2(M1PWM6) & PF3(M1PWM7) are used to generate PWM (up-down counting)
	// start by enabling the clock to the PWM Module (PWM1)
  HWREG(SYSCTL_RCGCPWM) |= SYSCTL_RCGCPWM_R1;

	// enable the clock to Port F
  HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R5;
	// enable the clock to Port E
  HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R4;

	// Select the PWM clock as System Clock/32
  HWREG(SYSCTL_RCC) = (HWREG(SYSCTL_RCC) & ~SYSCTL_RCC_PWMDIV_M) |
    (SYSCTL_RCC_USEPWMDIV | SYSCTL_RCC_PWMDIV_32);
  
	// make sure that the PWM module clock has gotten going
	while ((HWREG(SYSCTL_PRPWM) & SYSCTL_PRPWM_R1) != SYSCTL_PRPWM_R1)
    ;

	// disable the PWM while initializing
  HWREG( PWM1_BASE+PWM_O_3_CTL ) = 0; //(PWM Generator 3: PWM 6 & 7)
	HWREG( PWM1_BASE+PWM_O_2_CTL ) = 0; //(PWM Generator 2: PWM 4 & 5)
	HWREG( PWM1_BASE+PWM_O_1_CTL ) = 0; //(PWM Generator 1: PWM 2 & 3)

	// program generators to go to 1 at rising compare A/B, 0 on falling compare A/B 
  HWREG( PWM1_BASE+PWM_O_3_GENA) = GenA_Normal;
  HWREG( PWM1_BASE+PWM_O_3_GENB) = GenB_Normal;
	//HWREG( PWM1_BASE+PWM_O_2_GENA) = GenA_Normal;
	HWREG( PWM1_BASE+PWM_O_2_GENB) = GenB_Normal;
	HWREG( PWM1_BASE+PWM_O_1_GENB) = GenB_Normal;
	
	// Set the PWM period. Since we are counting both up & down, we initialize
	// the load register to 1/2 the desired total period. We will also program
	// the match compare registers to 1/2 the desired high time  
  HWREG( PWM1_BASE+PWM_O_3_LOAD) = ((PeriodInuS * PWMTicksPerMS)/1000)>>1; // Period
	HWREG( PWM1_BASE+PWM_O_2_LOAD) = ((PeriodInuS * PWMTicksPerMS)/1000)>>1; // Period
	HWREG( PWM1_BASE+PWM_O_1_LOAD) = ((PeriodInuS * PWMTicksPerMS)/1000)>>1; // Period
	
	// Set the initial Duty cycle on A & B to 0%  
	HWREG( PWM1_BASE+PWM_O_3_CMPA) = 0; // Duty cycle
	HWREG( PWM1_BASE+PWM_O_3_CMPB) = 0;
	//HWREG( PWM1_BASE+PWM_O_2_CMPA) = 0; // Duty cycle
	HWREG( PWM1_BASE+PWM_O_2_CMPB) = 0;
	HWREG( PWM1_BASE+PWM_O_1_CMPB) = 0;
	
	// enable the PWM outputs
  HWREG( PWM1_BASE+PWM_O_ENABLE) |= (PWM_ENABLE_PWM6EN | PWM_ENABLE_PWM7EN);
	HWREG( PWM1_BASE+PWM_O_ENABLE) |= (PWM_ENABLE_PWM3EN | PWM_ENABLE_PWM5EN);

	// now configure the Port F pins to be PWM outputs
	// start by selecting the alternate function for PF2 & 3
  HWREG(GPIO_PORTF_BASE+GPIO_O_AFSEL) |= (BIT1HI | BIT2HI | BIT3HI);
	HWREG(GPIO_PORTE_BASE+GPIO_O_AFSEL) |= BIT5HI;

	// now choose to map PWM to those pins, this is a mux value of 5 that we
	// want to use for specifying the function on bits 0 & 1 & 2 & 3
  HWREG(GPIO_PORTF_BASE+GPIO_O_PCTL) = 
    (HWREG(GPIO_PORTF_BASE+GPIO_O_PCTL) & 0xffff0000) + (5<<(3*BitsPerNibble)) +
      (5<<(2*BitsPerNibble)) + (5<<(1*BitsPerNibble)) + (5<<(0*BitsPerNibble));
			
	HWREG(GPIO_PORTE_BASE+GPIO_O_PCTL) = 
    (HWREG(GPIO_PORTE_BASE+GPIO_O_PCTL) & 0xff0fffff) + (5<<(5*BitsPerNibble));

	// Enable pins 0 & 1 & 2 & 3 on Port F for digital I/O
	HWREG(GPIO_PORTF_BASE+GPIO_O_DEN) |= (BIT0HI|BIT1HI|BIT2HI|BIT3HI);
	HWREG(GPIO_PORTE_BASE+GPIO_O_DEN) |= (BIT5HI);
	
	// make pins 0 & 1 & 2 & 3 on Port F into outputs
	HWREG(GPIO_PORTF_BASE+GPIO_O_DIR) |= (BIT0HI|BIT1HI|BIT2HI|BIT3HI);
	HWREG(GPIO_PORTE_BASE+GPIO_O_DIR) |= (BIT5HI);
  
	// set the up/down count mode, enable the PWM generator and make
	// both generator updates locally synchronized to zero count
  HWREG(PWM1_BASE+ PWM_O_3_CTL) = (PWM_3_CTL_MODE | PWM_3_CTL_ENABLE | 
                                    PWM_3_CTL_GENAUPD_LS | PWM_3_CTL_GENBUPD_LS);
	HWREG(PWM1_BASE+ PWM_O_2_CTL) = (PWM_2_CTL_MODE | PWM_2_CTL_ENABLE | 
                                    PWM_2_CTL_GENAUPD_LS | PWM_2_CTL_GENBUPD_LS);
	HWREG(PWM1_BASE+ PWM_O_1_CTL) = (PWM_1_CTL_MODE | PWM_1_CTL_ENABLE | 
                                    PWM_1_CTL_GENAUPD_LS | PWM_1_CTL_GENBUPD_LS);

}

void SetServoDuty(uint8_t ServoMotor, uint8_t Duty)
{
	if (ServoMotor == GATE_SERVO) {
		if (Duty == 0) {
			HWREG( PWM1_BASE+PWM_O_1_GENB) = PWM_1_GENB_ACTZERO_ZERO;
		} else if (Duty == 100) {
			HWREG( PWM1_BASE+PWM_O_1_GENB) = PWM_1_GENB_ACTZERO_ONE;
		} else {
			HWREG( PWM1_BASE+PWM_O_1_GENB) = GenB_Normal;
			HWREG( PWM1_BASE+PWM_O_1_CMPB) = HWREG( PWM1_BASE+PWM_O_1_LOAD) - (HWREG( PWM1_BASE+PWM_O_1_LOAD)*Duty/100);		
		}
	} else if (ServoMotor == SENSOR_SERVO) {
			if (Duty == 0) {
				HWREG( PWM1_BASE+PWM_O_2_GENB) = PWM_2_GENB_ACTZERO_ZERO;
			} else if (Duty == 100) {
				HWREG( PWM1_BASE+PWM_O_2_GENB) = PWM_2_GENB_ACTZERO_ONE;
			} else {
				HWREG( PWM1_BASE+PWM_O_2_GENB) = GenB_Normal;
				HWREG( PWM1_BASE+PWM_O_2_CMPB) = HWREG( PWM1_BASE+PWM_O_2_LOAD) - (HWREG( PWM1_BASE+PWM_O_2_LOAD)*Duty/100);		
			}
	} else if (ServoMotor == LEFT_ROLLER_SERVO) {
		if (Duty == 0) {
			HWREG( PWM1_BASE+PWM_O_3_GENA) = PWM_3_GENA_ACTZERO_ZERO;
		} else if (Duty == 100) {
			HWREG( PWM1_BASE+PWM_O_3_GENA) = PWM_3_GENA_ACTZERO_ONE;
		} else {
			HWREG( PWM1_BASE+PWM_O_3_GENA) = GenA_Normal;
			HWREG( PWM1_BASE+PWM_O_3_CMPA) = HWREG( PWM1_BASE+PWM_O_3_LOAD) - (HWREG( PWM1_BASE+PWM_O_3_LOAD)*Duty/100);		
		}
	} else if (ServoMotor == RIGHT_ROLLER_SERVO) {
			if (Duty == 0) {
				HWREG( PWM1_BASE+PWM_O_3_GENB) = PWM_3_GENB_ACTZERO_ZERO;
			} else if (Duty == 100) {
				HWREG( PWM1_BASE+PWM_O_3_GENB) = PWM_3_GENB_ACTZERO_ONE;
			} else {
				HWREG( PWM1_BASE+PWM_O_3_GENB) = GenB_Normal;
				HWREG( PWM1_BASE+PWM_O_3_CMPB) = HWREG( PWM1_BASE+PWM_O_3_LOAD) - (HWREG( PWM1_BASE+PWM_O_3_LOAD)*Duty/100);		
			}
	}
	else {
		puts("ERROR: Unable to set motor speed !!!\r\n");
	}
}

void OpenServoGate(void){
	SetServoDuty(GATE_SERVO,7);
}

void HalfOpenServoGate(void){
	SetServoDuty(GATE_SERVO,6);
}

void CloseServoGate(void){
	SetServoDuty(GATE_SERVO,5);
}

void ExpandRightRollerArm(void){
	SetServoDuty(RIGHT_ROLLER_SERVO,3);
}

void CloseRightRollerArm(void){
	SetServoDuty(RIGHT_ROLLER_SERVO,12);
}

void ExpandLeftRollerArm(void){
	SetServoDuty(LEFT_ROLLER_SERVO,12);
}

void CloseLeftRollerArm(void){
	SetServoDuty(LEFT_ROLLER_SERVO,3);
}

void ExpandSensorArm(void){
	SetServoDuty(SENSOR_SERVO,9);
}

void CloseSensorArm(void){
	SetServoDuty(SENSOR_SERVO,3);
};
/***************************************************************************
 private functions
 ***************************************************************************/

/*------------------------------- Footnotes -------------------------------*/
/*------------------------------ End of file ------------------------------*/

