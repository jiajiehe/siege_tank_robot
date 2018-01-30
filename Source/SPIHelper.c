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
#include "inc/hw_ssi.h"

// the headers to access the TivaWare Library
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"

#include "SPIService.h"

/*----------------------------- Module Defines ----------------------------*/
#define ALL_BITS (0xff<<2)
// set SSI clock to be 400kHz
#define CPSDVSR 16
#define SCR 249

// for convenience to write data to registers
#define BitsPerNibble 4

/*------------------------------ Public Functions ------------------------------*/
void SPIInit(void) {
// Enable the clock to the GPIO port (PA 2-5)
	HWREG(SYSCTL_RCGCGPIO)|= SYSCTL_RCGCGPIO_R0; // Port A
// Enable the clock to SSI module (SSI Module 0 Run Mode Clock)
	HWREG(SYSCTL_RCGCSSI) |= SYSCTL_RCGCSSI_R0;
// Wait for the GPIO port to be ready
	while ((HWREG(SYSCTL_RCGCGPIO) & SYSCTL_RCGCGPIO_R0) != SYSCTL_RCGCGPIO_R0);
// Program the GPIO to use the alternate functions on the SSI pins
	HWREG(GPIO_PORTA_BASE+GPIO_O_AFSEL) |= (BIT2HI | BIT3HI | BIT4HI | BIT5HI);
// Set mux position in GPIOPCTL to select the SSI use of the pins
	HWREG(GPIO_PORTA_BASE+GPIO_O_PCTL) =
	(HWREG(GPIO_PORTA_BASE+GPIO_O_PCTL) & 0xff0000ff) + (2<<(2*BitsPerNibble)) + (2<<(3*BitsPerNibble)) + (2<<(4*BitsPerNibble)) + (2<<(5*BitsPerNibble));
// Program the Port A lines for digital I/O
	HWREG(GPIO_PORTA_BASE+GPIO_O_DEN) |= (BIT2HI | BIT3HI | BIT4HI | BIT5HI);
// Program the required data directions on the port lines
	HWREG(GPIO_PORTA_BASE+GPIO_O_DIR) |= (BIT2HI | BIT3HI | BIT5HI); // PA2, 3, 5 are outputs (PA2: SCK, PA3: slave select, PA4: MISO)
	HWREG(GPIO_PORTA_BASE+GPIO_O_DIR) &= BIT4LO; // PA4 is an input
//If using SPI mode 3, program the pull-up on the clock line (PA2)
	HWREG(GPIO_PORTA_BASE+GPIO_O_PUR) |= BIT2HI;			
//Wait for the SSI0 to be ready
	while((HWREG(SYSCTL_RCGCSSI) & SYSCTL_RCGCSSI_R0) != SYSCTL_RCGCSSI_R0);
//Make sure that the SSI is disabled before programming mode bits
	HWREG(SSI0_BASE+SSI_O_CR1) &= ~SSI_CR1_SSE; 
//Select master mode (MS) & TXRIS indicating End of Transmit (EOT)
	HWREG(SSI0_BASE+SSI_O_CR1) &= ~SSI_CR1_MS; // Master is 0
	HWREG(SSI0_BASE+SSI_O_CR1) |= SSI_CR1_EOT; // End of Transimit is 1
//Configure the SSI clock source to the system clock
	HWREG(SSI0_BASE+SSI_O_CC) |= SSI_CC_CS_SYSPLL; // select to sys clock
//Configure the clock pre-scaler
	HWREG(SSI0_BASE+SSI_O_CPSR) |= CPSDVSR; // 16 define in this module

//Configure clock rate (SCR), phase & polarity (SPH, SPO), mode (FRF), data size (DSS)
	HWREG(SSI0_BASE+SSI_O_CR0) |= SCR<<8; // 249 define in this module, SCR need to shift
	HWREG(SSI0_BASE+SSI_O_CR0) |= SSI_CR0_SPH; // second edge, SPH = 1
	HWREG(SSI0_BASE+SSI_O_CR0) |= SSI_CR0_SPO; // idel at high, SPO = 1
	HWREG(SSI0_BASE+SSI_O_CR0) |= SSI_CR0_FRF_MOTO; // freescale SPI Frame Format // FRF
	HWREG(SSI0_BASE+SSI_O_CR0) |= SSI_CR0_DSS_8; // data size is 8 bits // DSS

//Locally enable interrupts (TXIM in SSIIM)
	HWREG(SSI0_BASE + SSI_O_IM) |= SSI_IM_TXIM;	
//Make sure that the SSI is enabled for operation
	HWREG(SSI0_BASE+SSI_O_CR1) |= SSI_CR1_SSE;
//Enable globally
	__enable_irq();
//Enable the NVIC interrupt for the SSI when starting to transmit
  HWREG(NVIC_EN0) |= BIT7HI;
	puts("/********** SPI Initialization Completed **********/\r\n");
}

void SPI_SendCMD(uint8_t command)
{
	HWREG(SSI0_BASE+SSI_O_DR) = command;
}

uint8_t SPI_ReadRES(void)
{
	uint8_t response;
	response = HWREG(SSI0_BASE+SSI_O_DR);
	return response;
}

void EOTISR(void) 
{
	// locally disable interrupt
	HWREG(SSI0_BASE+SSI_O_IM) &= ~SSI_IM_TXIM;
	// grab data from SSI data register
	//CurrentCommand = HWREG(SSI0_BASE+SSI_O_DR);
	// post EOT event to this service to handle it
	ES_Event ThisEvent;
	ThisEvent.EventType = SSI_EOT;
	PostSPIService(ThisEvent);
}
/*------------------------------- Footnotes -------------------------------*/
/*------------------------------ End of file ------------------------------*/
