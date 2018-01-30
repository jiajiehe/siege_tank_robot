/****************************************************************************
 Module
   SPIMaster.c

 Revision
   2.0.1

 Description
   This is a template for the top level Hierarchical state machine

 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 02/23/17 20:18 czhang94  Began coding    
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
#include "inc/hw_ssi.h"

// the headers to access the TivaWare Library
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"

// the headers from this and other services
#include "SPIHelper.h"
#include "SPIService.h"
#include "HallEffectService.h"
#include "MasterSM.h"

/*----------------------------- Module Defines ----------------------------*/
#define STATUS_QUERY 0xC0
#define REPORT_QUERY 0x70

#define QUERY_INTERVAL 20 // 10mS between 5-byte transfers
#define REPORT_RESEND_INTERVAL 1000 // if response not ready for 1S, resend the frequency

// response bytes code
#define SB1 1
#define SB2 2
#define SB3 3
#define REPORT 4
/*---------------------------- Module Functions ---------------------------*/
// During functions of sub-state machines (for further revision)
static ES_Event DuringWaiting2Send(ES_Event Event);
static ES_Event DuringWaiting4EOT(ES_Event Event);
static ES_Event DuringWaiting4ResponseReady( ES_Event Event);
static ES_Event DuringWaiting4Timeout(ES_Event Event);

/*---------------------------- Module Variables ---------------------------*/
// everybody needs a state variable, though if the top level state machine
// is just a single state container for orthogonal regions, you could get
// away without it
static SPIState_t CurrentState;
static uint8_t MyPriority;
static uint8_t Command;
static int StatusIndex2Return;
static uint8_t Response[5];
static uint32_t Response32bit;
static bool isResponseReady;
static uint8_t CurrentFreq2Report;

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitSPIService

 Parameters
     uint8_t : the priorty of this service

 Returns
     boolean, False if error in initialization, True otherwise

 Description
     Saves away the priority,  and starts
     the top level state machine
 Notes

 Author
     C. Zhang, 02/23/17, 20:34
****************************************************************************/
bool InitSPIService ( uint8_t Priority )
{
  ES_Event ThisEvent;

  MyPriority = Priority;  // save our priority
	
	// call all initialization functions below
	SPIInit();
	ES_Timer_Init(ES_Timer_RATE_1mS);

  ThisEvent.EventType = ES_ENTRY;
  // Start the Master State machine

  StartSPIService( ThisEvent );

  return true;
}

/****************************************************************************
 Function
     PostSPIService

 Parameters
     ES_Event ThisEvent , the event to post to the queue

 Returns
     boolean False if the post operation failed, True otherwise

 Description
     Posts an event to this state machine's queue
 Notes

 Author
     C. Zhang, 02/23/17, 20:35
****************************************************************************/
bool PostSPIService( ES_Event ThisEvent )
{
  return ES_PostToService( MyPriority, ThisEvent);
}

/****************************************************************************
 Function
    RunSPIService

 Parameters
   ES_Event: the event to process

 Returns
   ES_Event: an event to return

 Description
   the run function for the top level state machine 
 Notes
   uses nested switch/case to implement the machine.
 Author
   C. Zhang, 02/23/17, 20:35
****************************************************************************/
ES_Event RunSPIService( ES_Event CurrentEvent )
{
   bool MakeTransition = false;/* are we making a state transition? */
   SPIState_t NextState = CurrentState;
   ES_Event EntryEventKind = { ES_ENTRY, 0 };// default to normal entry to new state
   ES_Event ReturnEvent = { ES_NO_EVENT, 0 }; // assume no error

    switch ( CurrentState )
   {
       case Waiting2Send : 
         // Execute During function for state one. ES_ENTRY & ES_EXIT are
         // processed here allow the lowere level state machines to re-map
         // or consume the event
         CurrentEvent = DuringWaiting2Send(CurrentEvent); // 'virtual' during function
         //process any events
         if ( CurrentEvent.EventType != ES_NO_EVENT )
         {
            switch (CurrentEvent.EventType)
            {
               case SEND_CMD :
									if (CurrentEvent.EventParam < 4) { // MasterSM is asking for SB1, SB2 or SB3 (1-3)
										Command = STATUS_QUERY;
										StatusIndex2Return = CurrentEvent.EventParam;
										NextState = Waiting4EOT;
										puts("Querying status byte......\r\n");
									} else {
										CurrentFreq2Report = getFreqCode();
										printf("Mag field freq code = %d\r\n", CurrentFreq2Report);
										Command = 0x80 + CurrentFreq2Report;
										NextState = Waiting4ResponseReady;
										isResponseReady = false;
										puts("Reporting frequency.....\r\n");
										ES_Timer_InitTimer(REPORT_RESEND_TIMER, REPORT_RESEND_INTERVAL);
									}
                  
									HWREG(SSI0_BASE + SSI_O_IM) |= SSI_IM_TXIM;
									// Execute action function for Waiting2Send
							    SPI_SendCMD(Command);
							    SPI_SendCMD(0x00);
							    SPI_SendCMD(0x00);
							    SPI_SendCMD(0x00);
							    SPI_SendCMD(0x00);
								  for (int i = 0; i < 5; i++) { // clear it for a new response
										Response[i] = 0;
									}
									Response32bit = 0;
									uint8_t SSIRawINTStatus;
									SSIRawINTStatus = HWREG(SSI0_BASE+SSI_O_RIS);
								  printf("Command Sent,SSI0 raw interrupt status byte = %02x\r\n",SSIRawINTStatus);
                  // for internal transitions, skip changing MakeTransition
                  MakeTransition = true; //mark that we are taking a transition
                  break;
                // repeat cases as required for relevant events
							 default: break;
            }
         }
         break;
				 
				case Waiting4EOT : 
         // Execute During function for state one. ES_ENTRY & ES_EXIT are
         // processed here allow the lowere level state machines to re-map
         // or consume the event
         CurrentEvent = DuringWaiting4EOT(CurrentEvent); // 'virtual' during function
         //process any events
         if ( CurrentEvent.EventType != ES_NO_EVENT )
         {
            switch (CurrentEvent.EventType)
            {
               case SSI_EOT: 
                  // Execute action function for Waiting2Send
									for (int i = 0; i < 5; i++) {
										Response[i] = SPI_ReadRES(); // the five response bytes are indexed from 0 to 4
										printf("StatusResponse: 0x%02x\r\n", Response[i]);
									}
									
							    ES_Event ThisEvent;
									ThisEvent.EventType = ES_LOC_STATUS;                        
									ThisEvent.EventParam = Response[StatusIndex2Return + 1]; 
									PostMasterSM(ThisEvent);                                    
									
                  NextState = Waiting4Timeout;//Decide what the next state will be
                  // for internal transitions, skip changing MakeTransition
                  MakeTransition = true; //mark that we are taking a transition
									ES_Timer_InitTimer(TRANSFER_INTERVAL_TIMER, QUERY_INTERVAL); // 10mS between transfers
                  break;
									
								default: break;
            }
         }
         break;
				 
				 case Waiting4ResponseReady : 
         // Execute During function for state one. ES_ENTRY & ES_EXIT are
         // processed here allow the lowere level state machines to re-map
         // or consume the event
         CurrentEvent = DuringWaiting4ResponseReady(CurrentEvent); // 'virtual' during function
         if ( CurrentEvent.EventType != ES_NO_EVENT )
         {
            switch (CurrentEvent.EventType)
            {
							case SSI_EOT: 
									for (int i = 0; i < 5; i++) {
										Response[i] = SPI_ReadRES(); // the five response bytes are indexed from 0 to 4
										printf("ReportResponse: 0x%02x\r\n", Response[i]);
									}
							    if (Response[2] == 0xAA) { // check whether response is ready
										isResponseReady = true;
										ES_Timer_StopTimer(REPORT_RESEND_TIMER);
										puts("Response Ready!!!!\r");
										printf("ACK bits are: %02x\r\n", Response[3]>>6);
										
										ES_Event ThisEvent;
										ThisEvent.EventType = ES_LOC_RS;          
										ThisEvent.EventParam = Response[3]; 
										PostMasterSM(ThisEvent); 
									} else {
										puts("Response NOT ready!!!!\r\n");
										ES_Timer_InitTimer(REPORT_QUERY_TIMER, QUERY_INTERVAL);
									}
                  break;
									
							case ES_TIMEOUT:
								  if (CurrentEvent.EventParam == REPORT_QUERY_TIMER) {
										if (isResponseReady) {
											NextState = Waiting2Send;
                      MakeTransition = true;
											//puts("Going back to Waiting2Send\r\n");
										} else {
											ES_Event ThisEvent;
											ThisEvent.EventType = SEND_CMD;
											ThisEvent.EventParam = REPORT_QUERY; //0x70
											PostSPIService(ThisEvent);
											ES_Timer_InitTimer(REPORT_QUERY_TIMER, QUERY_INTERVAL);
										}
									}
									
									if (CurrentEvent.EventParam == REPORT_RESEND_TIMER) {
										CurrentFreq2Report = getFreqCode();
										uint8_t FreqCommand = 0x80 + CurrentFreq2Report;
										ES_Event ThisEvent;
										ThisEvent.EventType = SEND_CMD;
										ThisEvent.EventParam = FreqCommand;
										PostSPIService(ThisEvent);
										ES_Timer_InitTimer(REPORT_RESEND_TIMER, REPORT_RESEND_INTERVAL);
									}
								 break;
							 
							 case SEND_CMD:
								  SPI_SendCMD(CurrentEvent.EventParam);
							    SPI_SendCMD(0x00);
							    SPI_SendCMD(0x00);
							    SPI_SendCMD(0x00);
							    SPI_SendCMD(0x00);
									puts("Resending....\r\n");
							    HWREG(SSI0_BASE + SSI_O_IM) |= SSI_IM_TXIM;
								  for (int i = 0; i < 5; i++) { // clear it for a new response
										Response[i] = 0;
									}
								 break;
									
								default: break;
            }
         }
         break;
				 
				 case Waiting4Timeout : 
         // Execute During function for state one. ES_ENTRY & ES_EXIT are
         // processed here allow the lowere level state machines to re-map
         // or consume the event
         CurrentEvent = DuringWaiting4Timeout(CurrentEvent); // 'virtual' during function
         //process any events
         if ( CurrentEvent.EventType != ES_NO_EVENT )
         {
            switch (CurrentEvent.EventType)
            {
               case ES_TIMEOUT : 
                  if (CurrentEvent.EventParam == TRANSFER_INTERVAL_TIMER) {
										NextState = Waiting2Send;
                    MakeTransition = true; 
									}
                  break;
								
								default: break;
            }
         }
         break;
    }
    //   If we are making a state transition
    if (MakeTransition == true)
    {
       //   Execute exit function for current state
       CurrentEvent.EventType = ES_EXIT;
       RunSPIService(CurrentEvent);

       CurrentState = NextState; //Modify state variable

       // Execute entry function for new state
       // this defaults to ES_ENTRY
       RunSPIService(EntryEventKind);
     }
   // in the absence of an error the top level state machine should
   // always return ES_NO_EVENT, which we initialized at the top of func
   return(ReturnEvent);
}
/****************************************************************************
 Function
     StartSPIService

 Parameters
     ES_Event CurrentEvent

 Returns
     nothing

 Description
     Does any required initialization for this state machine
 Notes

 Author
     C. Zhang, 02/23/17, 22:46
****************************************************************************/
void StartSPIService ( ES_Event CurrentEvent )
{
  CurrentState = Waiting2Send;
	for (int i = 0; i < 5; i++) {
		Response[i] = 0;
	}
	Response32bit = 0;
  // now we need to let the Run function init the lower level state machines
  // use LocalEvent to keep the compiler from complaining about unused var
  RunSPIService(CurrentEvent);
  return;
}

uint32_t getResponse32bit(void)
{ 
	for (int i = 0; i < 5; i++) {
		Response32bit = (Response32bit<<8) + SPI_ReadRES();
	}
	return Response32bit;
}

/***************************************************************************
 private functions
 ***************************************************************************/

static ES_Event DuringWaiting2Send( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assme no re-mapping or comsumption

    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
        // implement any entry actions required for this state machine
        
        // after that start any lower level machines that run in this state
        //StartLowerLevelSM( Event );
        // repeat the StartxxxSM() functions for concurrent state machines
        // on the lower level
    }
    else if ( Event.EventType == ES_EXIT )
    {
        // on exit, give the lower levels a chance to clean up first
        //RunLowerLevelSM(Event);
        // repeat for any concurrently running state machines
        // now do any local exit functionality
      
    }else
    // do the 'during' function for this state
    {
        // run any lower level state machine
        // ReturnEvent = RunLowerLevelSM(Event);
      
        // repeat for any concurrent lower level machines
      
        // do any activity that is repeated as long as we are in this state
    }
    // return either Event, if you don't want to allow the lower level machine
    // to remap the current event, or ReturnEvent if you do want to allow it.
    return(ReturnEvent);
}

static ES_Event DuringWaiting4EOT( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assme no re-mapping or comsumption

    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
        // implement any entry actions required for this state machine
        
        // after that start any lower level machines that run in this state
        //StartLowerLevelSM( Event );
        // repeat the StartxxxSM() functions for concurrent state machines
        // on the lower level
    }
    else if ( Event.EventType == ES_EXIT )
    {
        // on exit, give the lower levels a chance to clean up first
        //RunLowerLevelSM(Event);
        // repeat for any concurrently running state machines
        // now do any local exit functionality
      
    }else
    // do the 'during' function for this state
    {
        // run any lower level state machine
        // ReturnEvent = RunLowerLevelSM(Event);
      
        // repeat for any concurrent lower level machines
      
        // do any activity that is repeated as long as we are in this state
    }
    // return either Event, if you don't want to allow the lower level machine
    // to remap the current event, or ReturnEvent if you do want to allow it.
    return(ReturnEvent);
}

static ES_Event DuringWaiting4ResponseReady( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assme no re-mapping or comsumption

    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
        // implement any entry actions required for this state machine
        
        // after that start any lower level machines that run in this state
        //StartLowerLevelSM( Event );
        // repeat the StartxxxSM() functions for concurrent state machines
        // on the lower level
    }
    else if ( Event.EventType == ES_EXIT )
    {
        // on exit, give the lower levels a chance to clean up first
        //RunLowerLevelSM(Event);
        // repeat for any concurrently running state machines
        // now do any local exit functionality
      
    }else
    // do the 'during' function for this state
    {
        // run any lower level state machine
        // ReturnEvent = RunLowerLevelSM(Event);
      
        // repeat for any concurrent lower level machines
      
        // do any activity that is repeated as long as we are in this state
    }
    // return either Event, if you don't want to allow the lower level machine
    // to remap the current event, or ReturnEvent if you do want to allow it.
    return(ReturnEvent);
}

static ES_Event DuringWaiting4Timeout( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assme no re-mapping or comsumption

    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
        // implement any entry actions required for this state machine
        
        // after that start any lower level machines that run in this state
        //StartLowerLevelSM( Event );
        // repeat the StartxxxSM() functions for concurrent state machines
        // on the lower level
    }
    else if ( Event.EventType == ES_EXIT )
    {
        // on exit, give the lower levels a chance to clean up first
        //RunLowerLevelSM(Event);
        // repeat for any concurrently running state machines
        // now do any local exit functionality
      
    }else
    // do the 'during' function for this state
    {
        // run any lower level state machine
        // ReturnEvent = RunLowerLevelSM(Event);
      
        // repeat for any concurrent lower level machines
      
        // do any activity that is repeated as long as we are in this state
    }
    // return either Event, if you don't want to allow the lower level machine
    // to remap the current event, or ReturnEvent if you do want to allow it.
    return(ReturnEvent);
}
