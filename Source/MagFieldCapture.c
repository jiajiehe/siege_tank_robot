
/******* Module level static variables ********/
/* For Input capture */
static uint32_t Period =0;
static uint32_t LastCapture =0;

/* For MagField Timer*/
static bool YesNewEdge = false;

/************Start of Input Capture functions *******************/

void InitMagFieldCapture(void)
{
	// Pin in use: PD6, WT5CCP0 (Wide Timer 5 A)
	// start by enabling the clock to the timer (Wide Timer 5)
	HWREG(SYSCTL_RCGCWTIMER) |= SYSCTL_RCGCWTIMER_R5;
		// kill a few cycles to let the clock get going
	while((HWREG(SYSCTL_PRWTIMER) & SYSCTL_PRWTIMER_R0) != SYSCTL_PRWTIMER_R0)
	{
	}
	// enable the clock to Port D
	HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R3;
	// Wait until the port to be ready to accept new modifications
	while ( (HWREG(SYSCTL_PRGPIO) & (SYSCTL_RCGCGPIO_R3)) != (SYSCTL_RCGCGPIO_R3) ){};

	// make sure that timer (Timer A) is disabled before configuring
	HWREG(WTIMER5_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TAEN;
	// set it up in 32bit wide (individual, not concatenated) mode
	// the constant name derives from the 16/32 bit timer, but this is a 32/64
	// bit timer so we are setting the 32bit mode
	HWREG(WTIMER5_BASE+TIMER_O_CFG) = TIMER_CFG_16_BIT;
	// we want to use the full 32 bit count, so initialize the Interval Load
	// register to 0xffff.ffff (its default value :-) 107 seconds if using 40MHz
	HWREG(WTIMER5_BASE+TIMER_O_TAILR) = 0xffffffff;
	// set up timer A in capture mode (TAMR=3, TAAMS = 0),
	// for edge time (TACMR = 1) and up-counting (TACDIR = 1)
	HWREG(WTIMER5_BASE+TIMER_O_TAMR) =
	(HWREG(WTIMER5_BASE+TIMER_O_TAMR) & ~TIMER_TAMR_TAAMS) |
	(TIMER_TAMR_TACDIR | TIMER_TAMR_TACMR | TIMER_TAMR_TAMR_CAP);
	// To set the event to rising edge, we need to modify the TAEVENT bits
	// in GPTMCTL. Positive edges = 00, negative edges = 01. Clearing the 2 TAEVENT bits.
	HWREG(WTIMER5_BASE+TIMER_O_CTL) &= ~TIMER_CTL_TAEVENT_M;
	// Now Set up the port to do the capture (clock was enabled earlier)
	// start by setting the alternate function for Port D bit 6 (WT5CCP0)
	HWREG(GPIO_PORTD_BASE+GPIO_O_AFSEL) |= BIT6HI;
	// Then, map bit 6's alternate function to WT5CCP0
	// 7 is the mux value to select WT5CCP0, 24 to shift it over to the
	// right nibble for bit 6 (4 bits/nibble * 6 bits)
	HWREG(GPIO_PORTD_BASE+GPIO_O_PCTL) =
	(HWREG(GPIO_PORTD_BASE+GPIO_O_PCTL) & 0xfff0ffff) + (7<<24);
	// Enable pin on Port D for digital I/O
	HWREG(GPIO_PORTD_BASE+GPIO_O_DEN) |= BIT6HI;
	// make pin 6 on Port D into an input
	HWREG(GPIO_PORTD_BASE+GPIO_O_DIR) &= BIT4LO;
	// back to the timer to enable a local capture interrupt
	HWREG(WTIMER5_BASE+TIMER_O_IMR) |= TIMER_IMR_CAEIM;
	// enable the Timer A in Wide Timer 5 interrupt in the NVIC
	// it is interrupt number 92 so appears in EN2 at bit 28
	HWREG(NVIC_EN2) |= BIT28HI;
	// make sure interrupts are enabled globally
	__enable_irq();
	// now kick the timer off by enabling it and enabling the timer to
	// stall while stopped by the debugger
	HWREG(WTIMER5_BASE+TIMER_O_CTL) |= (TIMER_CTL_TAEN | TIMER_CTL_TASTALL);
}


void MagFieldCaptureResponse( void )
{
		uint32_t ThisCapture;
		// start by clearing the source of the interrupt, the input capture event
		HWREG(WTIMER5_BASE+TIMER_O_ICR) = TIMER_ICR_CAECINT;
		// now grab the captured value
		ThisCapture = HWREG(WTIMER5_BASE+TIMER_O_TAR);
		Period = ThisCapture - LastCapture;
		// update LastCapture to prepare for the next edge
		LastCapture = ThisCapture;
		
		// New edge detected, so...
		// 1) Update a state variable that RPMService and UpdateLEDs function can see to know whether
		// there is a new edge so that RPMService can write RPM to the TeraTerm and LEDs be updated
		YesNewEdge = true;
		// 2) Restart the OneShotTimer so that it doesn't timeout and doesn't set YesNewEdge to false
//		HWREG(WTIMER5_BASE+TIMER_O_TAV) = HWREG(WTIMER5_BASE+TIMER_O_TAILR);
//		HWREG(WTIMER5_BASE+TIMER_O_CTL) |= (TIMER_CTL_TAEN | TIMER_CTL_TASTALL);
		ES_Timer_InitTimer(MagFieldTimer,20); // 20 ms
}

/************End of Input Capture functions *******************/