/****************************************************************************
 Module
   d:\me218bTNG\Lectures\Lecture31\SMTemplateMain.c

 Revision
   1.0.1

 Description
   This is a template file for a main() implementing Hierarchical 
   State Machines within the Events and Services Framework.

 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 02/06/15 13:21 jec      minor tweaks to include header files & clock init for 
                         Tiva
 02/08/12 10:32 jec      major re-work for the Events and Services Framework
                         Gen2
 03/03/10 00:36 jec      now that StartTemplateSM takes an event as input
                         you should pass it something.
 03/17/09 10:20 jec      added cast to return from RunTemplateSM() to quiet
                         warnings because now that function returns Event_t
 02/11/05 16:56 jec      Began coding
****************************************************************************/
#include <stdint.h>
#include <stdbool.h>

#include "driverlib/sysctl.h"

#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_Timers.h"

#define clrScrn() 	puts("\x1b[2J")

int main (void)
{
  ES_Return_t ErrorType;
    
// Your hardware initialization function calls go here
// Set the clock to run at 40MhZ using the PLL and 16MHz external crystal
  SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN
			| SYSCTL_XTAL_16MHZ);

  // Initialize the terminal for puts/printf debugging
  TERMIO_Init();
	clrScrn();
   
// When doing testing, it is useful to announce just which program
// is running.
	puts("\rStart Testing \r");
	printf("the 2nd Generation Events & Services Framework V2.2\r\n");
	printf("Integration of SPIService, Hall Effect sensor and DrivingSM t\r\n");
	printf("%s %s\n",__TIME__, __DATE__);
	printf("\n\r\n");

// now initialize the Events and Services Framework and start it running
  ErrorType = ES_Initialize(ES_Timer_RATE_1mS);
  if ( ErrorType == Success ) {
    ErrorType = ES_Run();
  }
//if we got to here, there was an error
  switch (ErrorType){
    case FailedPointer:
      puts("Failed on NULL pointer");
      break;
    case FailedInit:
      puts("Failed Initialization");
      break;
    default:
      puts("Other Failure");
      break;
  }
  for(;;)   // hang after reporting error
    ;
}
