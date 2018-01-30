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

#define ALL_BITS (0xff<<2)

bool DetermineColor(void) {
 bool Color;
 // Initialize the port line PE2 to monitor button
	HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R4;
    // Wait until clock is ready
    while ((HWREG(SYSCTL_PRGPIO) & SYSCTL_PRGPIO_R4) != SYSCTL_PRGPIO_R4);
    // Set bit 2 on Port F to be used as digital I/O lines
    HWREG(GPIO_PORTE_BASE+GPIO_O_DEN) |= GPIO_PIN_2;
    // Set bit 2 on Port F to be input
    HWREG(GPIO_PORTE_BASE+GPIO_O_DIR) &= ~GPIO_PIN_2;
	
	if ((HWREG(GPIO_PORTE_BASE+(GPIO_O_DATA+ALL_BITS)) & BIT2HI) == BIT2HI) {
		Color = true;
		puts("Color = true\r\n");
	}

	if ((HWREG(GPIO_PORTE_BASE+(GPIO_O_DATA+ALL_BITS)) & BIT2HI) == 0) {
		Color = false;
		puts("Color = false\r\n");
	}
	return Color;
}
