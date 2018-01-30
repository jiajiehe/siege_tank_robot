/****************************************************************************
 Module
   Shooting.c

 Revision
   1.0

 Description
 Includes 1) shoot
					2) transit to GetBalls if balls run out
 
 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 02/28/17 18:44 ZS      
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/

// Include standard libraries in C
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Basic includes for a program using the Events and Services Framework
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_Events.h"
#include "ES_Timers.h"

/* include header files for this state machine as well as any machines at the
   next lower level in the hierarchy that are sub-machines to this machine
*/
#include "MasterSM.h"
#include "Shooting.h"
#include "SPIService.h"
#include "DecipherFunctions.h"
#include "DCMotorService.h"
#include "ServoGateService.h"
#include "FlywheelTest.h"
#include "COWSupplementService.h"
#include "LEDService.h"

/*----------------------------- Module Defines ----------------------------*/
// define constants for the states for this machine
// and any other local defines

#define ENTRY_STATE FlywheelRamping
#define RAMP_UP_TIME 1000 // 1 s
#define BALL_TRAVEL_TIME 5000 // 5 s
#define SHOOTING_WINDOW 21000 //22 s

// Identify motor direction
#define FWD 0
#define BWD 1
                                                                                                                                               
/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this machine, things like during
   functions, entry & exit functions.They should be functions relevant to the
   behavior of this state machine
*/

static ES_Event DuringRamping( ES_Event Event);
static ES_Event DuringScoring( ES_Event Event);
static ES_Event DuringGetCOWs( ES_Event Event);

/*---------------------------- Module Variables ---------------------------*/
// Every SM needs a "CurrentState"
static ShootingState_t CurrentShootingState;
static bool CurrentColor; // 0-Green, 1-Red
static uint8_t LastScore = 0;
static uint8_t COW_Number = 100;  // Initialized to 5 balls
static uint8_t Speed = 40;
static bool CurrentDirection;
/*------------------------------ Module Code ------------------------------*/

/****************************************************************************
 Function
    RunDrivingSM

 Parameters
   ES_Event: the event to process

 Returns
   ES_Event: an event to return

 Description
   A sub-state machine under MasterSM
 Notes
   uses nested switch/case to implement the machine.
 Author
   J. Edward Carryer, 2/11/05, 10:45AM
****************************************************************************/

ES_Event RunShooting( ES_Event CurrentEvent )
{
   bool MakeTransition = false;/* are we making a state transition? */
   ShootingState_t NextShootingState = CurrentShootingState;
   ES_Event EntryEventKind = { ES_ENTRY, 0 };// default to normal entry to new state
   ES_Event ReturnEvent = CurrentEvent; // assume no consumption of passed in event

   switch ( CurrentShootingState )
   {
       case FlywheelRamping :   // Current state is to ramp up the flywheel and get it stable
				 puts("Current Shooting State = FlywheelRamping\r\n");
         CurrentEvent = DuringRamping(CurrentEvent);
         // DuringRamping (entry function) starts a warm-up timer for the flywheel
			   if ( CurrentEvent.EventType != ES_NO_EVENT ) //If an event is active
         {
					 if (CurrentEvent.EventType == ES_TIMEOUT && CurrentEvent.EventParam == RampUpTimer)
					 {
						  puts("Ramp-up done. Launch 1st ball\r\n");
						  NextShootingState = Scoring;
						  MakeTransition = true;
							ReturnEvent.EventType = ES_NO_EVENT;  // consume this event so MasterSM sees no_event
					 }
					 else if (CurrentEvent.EventType == ES_TIMEOUT && CurrentEvent.EventParam == ShootingTimer)
					 {
						 	puts("Shooting timer expires during FlywheelRamping. Stop flywheel!\r\n");
						 	ReturnEvent.EventType = ES_NO_EVENT;  // consume this event
							// Flywheel will be turned off here instead of in the exit function of FlywheelRamping
							ES_Event StopFWEvent;
							StopFWEvent.EventType = ES_STOPFLYWHEEL;
							PostFlywheelTest(StopFWEvent);
						  // Notify MasterSM the shooting period is over
						  puts("Post to MasterSM ES_SHOOTING_OVER\r\n");
						  ES_Event ThisEvent;
						  ThisEvent.EventType = ES_SHOOTING_OVER;
							PostMasterSM(ThisEvent);
					 }
         }
				 else // Current Event is now ES_NO_EVENT. Correction 2/20/17 
         {     //Probably means that CurrentEvent was consumed by lower level
            ReturnEvent = CurrentEvent; // in that case update ReturnEvent too.
         }
       break;

       case Scoring :       // If current state is to launch shooting
				 puts("Current Shooting State = Scoring\r\n");
         CurrentEvent = DuringScoring(CurrentEvent);
         // DuringScoring (entry function) launches a ball and starts a ball-travel timer
			   // Check COW_Number
			   
         if ( CurrentEvent.EventType != ES_NO_EVENT ) //If an event is active
         {
					 if (CurrentEvent.EventType == ES_TIMEOUT && CurrentEvent.EventParam == BallTravelTimer)
					 {
						 puts("Ball Travel Timer expires\r\n");
							bool FreeShooting;
							FreeShooting = Query_isFreeShooting();
							if (COW_Number == 0)
							{
									puts("COWs run out. Go supplement COWs\r\n");
									// Flywheel will be turned off in the exit function of Scoring
									NextShootingState = GetCOWs;
									MakeTransition = true;
									ReturnEvent.EventType = ES_NO_EVENT;  // consume this event so MasterSM sees no_event
							}
							
							else
							{
								if (FreeShooting)
								{
									puts("Now enter free shooting state!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
									puts("BallTravelTimer ( 2s) expires. Keep launching COWs in the free shooting period!\r\n");
									// Launch another shooting
									ES_Event LaunchEvent;
									LaunchEvent.EventType = ES_OPENGATE;
									PostServoGateService(LaunchEvent);
									// Blink LED
									ES_Event LEDEvent;
									LEDEvent.EventType = ES_QUERYBALL;
									PostLEDService(LEDEvent);
									// Update COW counts
									COW_Number --;
									printf("Current COW number  = %d\r\n",COW_Number);
									ES_Timer_InitTimer(BallTravelTimer, BALL_TRAVEL_TIME/3);
									ES_Timer_StopTimer(ShootingTimer);
									puts("Start ball travel timer!\r\n");
								}
								
								else if (FreeShooting == false)
								{
									puts("BallTravelTimer ( 2s) expires. Ask LOC for score\r\n");
									// Poll LOC to ask for the score (of current color)
									ES_Event ThisEvent;
									ThisEvent.EventType = SEND_CMD;
									if (CurrentColor)  // current color = 1 -> red
									{
										ThisEvent.EventParam = SB3;  // Red score in status byte 3
										puts("Now ask for the RED side score!!\r\n");
									}
									else
									{
										ThisEvent.EventParam = SB2;  // Green score in status byte 2
									  puts("Now ask for the GREEN side score!!\r\n");
									}
									PostSPIService(ThisEvent);
								}
							}
							// Consume this event (does nothing but wait for LOC response)
							ReturnEvent.EventType = ES_NO_EVENT;  // Wants MasterSM to see NO_EVENT
					 }
					 else if (CurrentEvent.EventType == ES_LOC_STATUS)
					 {
						 uint8_t CurrentScore;
						 CurrentScore = DecipherScore(CurrentEvent.EventParam);
						 printf("CurrentScore = %d, LastScore = %d, for color %d\r\n",CurrentScore, LastScore, CurrentColor);
						 if (CurrentScore > LastScore) // Score successfully
						 {
							 puts("Score++! Post ES_SCORE to MasterSM\r\n");
							 // Notify MasterSM that we have scored
							 ES_Event ThisEvent;
							 ThisEvent.EventType = ES_SCORE; 
							 PostMasterSM(ThisEvent);
							 // When the MasterSM state transits from shooting to driving, exit function of shooting,
							 // and then exit function of scoring will be excecuted, thus the flywheel will be turned off.
						 }
						 else  // Failed scoring
						 {
							 // stay in Scoring, and shoot again
							 puts("Score didn't increase. Launch another COW!\r\n");
							 ES_Event LaunchEvent;
							 LaunchEvent.EventType = ES_OPENGATE;
							 PostServoGateService(LaunchEvent);
							 // Update COW counts
							 COW_Number --;
							 printf("Current COW number  = %d\r\n",COW_Number);
							 ES_Timer_InitTimer(BallTravelTimer, BALL_TRAVEL_TIME);
							 puts("Start ball travel timer!\r\n");
						 }
						 ReturnEvent.EventType = ES_NO_EVENT;  // Consume this ES_LOC_STATUS event
						 LastScore = CurrentScore;
					 }
					 else if (CurrentEvent.EventType == ES_TIMEOUT && CurrentEvent.EventParam == ShootingTimer)
					 {
						 	puts("Shooting timer expires during Scoring. Stop flywheel!\r\n");			 
							ReturnEvent.EventType = ES_NO_EVENT;  // consume this event
						  // Notify MasterSM the shooting period is over
						  ES_Event ThisEvent;
						  ThisEvent.EventType = ES_SHOOTING_OVER; 
						  // When the MasterSM state transits from shooting to driving, exit function of shooting,
							// and then exit function of scoring will be excecuted, thus the flywheel will be turned off.
							PostMasterSM(ThisEvent);
					 }
         }
				 else
         {     //Probably means that CurrentEvent was consumed by lower level
            ReturnEvent = CurrentEvent; // in that case update ReturnEvent too.
         }
       break;
				 
			 case GetCOWs :
				 puts("Current Shooting State = GetCOWs\r\n");
         CurrentEvent = DuringGetCOWs(CurrentEvent);
			 	 if ( CurrentEvent.EventType != ES_NO_EVENT ) //If an event is active
         {
					 if (CurrentEvent.EventType == ES_MOTOR_STALL)
					 {
						  puts("Hit the Supply Depot wall! Stop motors!\r\n");
						  // Stop
						  Stop();
						  ES_Event GetCOWsEvent;
						  GetCOWsEvent.EventType = ES_RELOAD;
						  PostCOWSupplementService(GetCOWsEvent);
							ReturnEvent.EventType = ES_NO_EVENT;  // consume this event so MasterSM sees no_event
					 }
					 else if (CurrentEvent.EventType == ES_FULL_LOAD)
					 {
						 	puts("Got four COWs!\r\n");
						  // Update COW_Number
						  COW_Number = 5;
						  // Ask for next active stage
						  ES_Event QueryLOCEvent;
							QueryLOCEvent.EventType = SEND_CMD;
							QueryLOCEvent.EventParam = SB1;
						  PostSPIService(QueryLOCEvent);
							ReturnEvent.EventType = ES_NO_EVENT; // Consume this ES_FULL_LOAD event
					 }
					 else if (CurrentEvent.EventType == ES_LOC_STATUS)
					 {
						 bool isStillShooting;
						 isStillShooting = DecipherDestinationType(CurrentEvent.EventParam,CurrentColor);
						 if (isStillShooting)
						 {
							 // This is rare, but if after refilling COWs it's still within the 20 shooting window,
							 // then first delay for 10 ms (ByteTransferIntervalTimer), and query for next active stage again
							 ES_Timer_InitTimer(ByteTransferIntervalTimer, 10);
						 }
						 else
						 {
							 uint8_t NextStage;
							 NextStage = DecipherDestination(CurrentEvent.EventParam,CurrentColor);
							 printf("Next active stage = %d\r\n", NextStage);
							 ReturnEvent.EventType = ES_NO_EVENT; // Consume this ES_LOC_STATUS event
							 
							 ES_Event GetCOWDone;
							 GetCOWDone.EventType = ES_BACK_FROM_DEPOT;
							 PostMasterSM(GetCOWDone);
						 }							 
					 }
					 
					 else if (CurrentEvent.EventType == ES_TIMEOUT && CurrentEvent.EventParam == ByteTransferIntervalTimer)
					 {
						 	// Ask for next active stage
						  puts("ByteTransferInterval times out, ask for next active stage again\r\n");
						  ES_Event QueryLOCEvent;
							QueryLOCEvent.EventType = SEND_CMD;
							QueryLOCEvent.EventParam = SB1;
						  PostSPIService(QueryLOCEvent);
							ReturnEvent.EventType = ES_NO_EVENT; // Consume this ES_FULL_LOAD event
					 }
					 
         }
				 else // Current Event is now ES_NO_EVENT. Correction 2/20/17 
         {     //Probably means that CurrentEvent was consumed by lower level
            ReturnEvent = CurrentEvent; // in that case update ReturnEvent too.
         }			 
			 break;
    }
    //   If we are making a state transition
    if (MakeTransition == true)
    {
       //   Execute exit function for current state
       CurrentEvent.EventType = ES_EXIT;
       RunShooting(CurrentEvent);

       CurrentShootingState = NextShootingState; //Modify state variable

       //   Execute entry function for new state
       // this defaults to ES_ENTRY
       RunShooting(EntryEventKind);
     }
     return(ReturnEvent);
}
/****************************************************************************
 Function
     StartTemplateSM

 Parameters
     None

 Returns
     None

 Description
     Does any required initialization for this state machine
 Notes

 Author
     J. Edward Carryer, 2/18/99, 10:38AM
****************************************************************************/
void StartShooting ( ES_Event CurrentEvent )
{
   // to implement entry to a history state or directly to a substate
   // you can modify the initialization of the CurrentShootingState variable
   // otherwise just start in the entry state every time the state machine
   // is started
	 
	 CurrentColor = QueryColor();
   if ( ES_ENTRY_HISTORY != CurrentEvent.EventType )
   {
        CurrentShootingState = ENTRY_STATE;
   }
   // call the entry function (if any) for the ENTRY_STATE
   RunShooting(CurrentEvent);
}

/****************************************************************************
 Function
     QueryTemplateSM

 Parameters
     None

 Returns
     TemplateState_t The current state of the Template state machine

 Description
     returns the current state of the Template state machine
 Notes

 Author
     J. Edward Carryer, 2/11/05, 10:38AM
****************************************************************************/
ShootingState_t QueryShooting ( void )
{
   return(CurrentShootingState);
}

/***************************************************************************
 private functions
 ***************************************************************************/

static ES_Event DuringRamping( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assme no re-mapping or comsumption

    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
      // No Start functions of lower level SM need to run
			// Local entry function: 1) start a 500 ms timer to get the flywheel running stably
			//											 2) ramp up the flywheel
			puts("Start RampUpTimer!\r\n");
			ES_Timer_InitTimer(ShootingTimer, SHOOTING_WINDOW);
			ES_Timer_InitTimer(RampUpTimer,RAMP_UP_TIME);
			ES_Event RampEvent;
			RampEvent.EventType = ES_RUNFLYWHEEL;
			PostFlywheelTest(RampEvent);
			puts("******Started flywheel********\r\n");
			PostFlywheelTest(RampEvent);
    }
    else if ( Event.EventType == ES_EXIT )
    {
      // No RunLowerLevelSM(Event) needed;
      // Local exit functionality: stop the flywheel
			puts("Doing exit function of ramping (no actual actions)\r\n");
      
    }else
    // do the 'during' function for this state
    {
        // No RunLowerLevelSM(Event) needs to run              
    }
    return(ReturnEvent);
}

static ES_Event DuringScoring( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assume no re-mapping or consumption

    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
				// Launch a shooting
				ES_Event LaunchEvent;
				LaunchEvent.EventType = ES_OPENGATE;
				PostServoGateService(LaunchEvent);
				// Update COW counts
				COW_Number --;
				printf("Current COW number  = %d\r\n",COW_Number);
				ES_Timer_InitTimer(BallTravelTimer, BALL_TRAVEL_TIME);
			  puts("Start ball travel timer!\r\n");
        // after that start any lower level machines that run in this state
        // repeat the StartxxxSM() functions for concurrent state machines
        // on the lower level
    }
    else if ( Event.EventType == ES_EXIT )
    {
      // No RunLowerLevelSM(Event) needed;
			puts("Exit function of Scoring: turn off flywheel\r\n");
			ES_Event StopFWEvent;
			StopFWEvent.EventType = ES_STOPFLYWHEEL;
			PostFlywheelTest(StopFWEvent);
			
    }else
    // do the 'during' function for this state
    {
        // run any lower level state machine
        //ReturnEvent.EventType = RunStartMoveToDestination(Event);
      
        // repeat for any concurrent lower level machines
      
        // do any activity that is repeated as long as we are in this state
    }
    return(ReturnEvent);
}

static ES_Event DuringGetCOWs( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assme no re-mapping or comsumption

    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
      // No Start functions of lower level SM need to run
			// Local entry function: post an event to COWSupplementService
			puts("First move to RELOAD location\r\n");
			//Call drive function in DCMotorService
			if (CurrentColor == RED)
			{
				CurrentDirection = FWD;
				Drive(Speed,FWD);
			}
			else
			{
				CurrentDirection = BWD;
				Drive(Speed,BWD);
			}
    }
    else if ( Event.EventType == ES_EXIT )
    {
      // No RunLowerLevelSM(Event) needed;
      // Local exit functionality: stop the flywheel
			puts("Doing exit function of GetCOWs (no actual actions)\r\n");
      
    }else
    // do the 'during' function for this state
    {
        // No RunLowerLevelSM(Event) needs to run              
    }
    return(ReturnEvent);
}
/******** Function to pass module level static variable to sub machines *****/
