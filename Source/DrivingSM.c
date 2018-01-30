/****************************************************************************
 Module
   DrivingSM.c

 Revision
   1.0

 Description
   Under Master state machine. In parallel with WaitToStartGame, Shooting, GetBalls.
	 Contains WaitToMove, MovingToSA, MovingToShootingSpot

 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 02/28/17 18:49 ZS      
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
#include "DrivingSM.h"
#include "MoveToDestination.h"
#include "SPIService.h"
#include "DecipherFunctions.h"
#include "DCMotorService.h"
#include "ServoGateService.h"

/*----------------------------- Module Defines ----------------------------*/
// define constants for the states for this machine
// and any other local defines

#define ENTRY_STATE WaitToMove
#define START_POSITION 32.5 //inches
#define SHOOTING_X 10 // assume shoot from 10 inches from the wall
#define FIFTEEN_INCH 15 // Need measurement and calibration
#define TEN_INCH 10 // Need measurement and calibration
#define ByteTransferInterval 10 //10 ms
#define SHOOTING_WINDOW 20000  // 20 s
#define ArmsExpansionTime 1100  // 1.1 s

// Identify motor direction
#define FWD 0
#define BWD 1
                                       																
/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this machine, things like during
   functions, entry & exit functions.They should be functions relevant to the
   behavior of this state machine
*/

static ES_Event DuringWaitToMove(ES_Event Event);
static ES_Event DuringMoveToDestination(ES_Event Event);

/*---------------------------- Module Variables ---------------------------*/
// Every SM needs a "CurrentState"
static DrivingSMState_t CurrentDrState;
static bool DestinationType;
static uint8_t StartingStage;
static bool CurrentColor; // 0-Green, 1-Red
static bool isFirstCycle = true;
static bool CurrentDirection;
// For now just manually set the color
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

ES_Event RunDrivingSM( ES_Event CurrentEvent )
{
   bool MakeTransition = false;/* are we making a state transition? */
   DrivingSMState_t NextDrState = CurrentDrState;
   ES_Event EntryEventKind = { ES_ENTRY, 0 };// default to normal entry to new state
   ES_Event ReturnEvent = CurrentEvent; // assume no consumption of passed in event

   switch ( CurrentDrState )
   {
       case WaitToMove :  // default starting state in DrivingSM
				 puts("Current Driving State = WaitToMove\r\n");
         CurrentEvent = DuringWaitToMove(CurrentEvent);
         // DuringWaitToMove only does querying LOC and
			   if ( CurrentEvent.EventType != ES_NO_EVENT ) //If an event is active
         {
					 if (CurrentEvent.EventType == ES_TIMEOUT && CurrentEvent.EventParam == ArmsExpansionTimer)
					 {
						 // Expand the arms
						 ES_Event ExpandLeftArmEvent;
						 ExpandLeftArmEvent.EventType = ES_EXPAND_LEFT_ROLLERARM;
						 PostServoGateService(ExpandLeftArmEvent);
						 
						 ES_Event ExpandRightArmEvent;
						 ExpandRightArmEvent.EventType = ES_EXPAND_RIGHT_ROLLERARM;
						 PostServoGateService(ExpandRightArmEvent);
						 
						 ES_Event ExpandSensorArmEvent;
						 ExpandSensorArmEvent.EventType = ES_EXPAND_SENSORARM;
						 PostServoGateService(ExpandSensorArmEvent);
						 
						 isFirstCycle = false;  // To avoid expanding arms when not necessary
						 // Start a 10 ms ByteTransferTimer before querying LOC for target stage
						 ES_Timer_InitTimer(ByteTransferIntervalTimer,ByteTransferInterval);
						 ReturnEvent.EventType = ES_NO_EVENT;  // Wants MasterSM to see NO_EVENT
					 }
					 else if (CurrentEvent.EventType == ES_TIMEOUT && CurrentEvent.EventParam == ByteTransferIntervalTimer)
					 {
						 	// Post an event to LOC to query active staging area (4 LSBs in status byte 1)
							ES_Event SACheckingEvent;
							SACheckingEvent.EventType = SEND_CMD;
							SACheckingEvent.EventParam = SB1;
							PostSPIService(SACheckingEvent);
							puts("Sent a request to LOC to check SB1\r\n");
							// Consume this event (does nothing but wait for LOC response)
							ReturnEvent.EventType = ES_NO_EVENT;  // Wants MasterSM to see NO_EVENT
					 }
					 else if (CurrentEvent.EventType == ES_LOC_STATUS) // If LOC posts an event about status byte 1 ( byte in its param) 
					 {
						  printf("CurrentColor = %d (0-Green,1-Red)\r\n",CurrentColor);
						  DestinationType = DecipherDestinationType(CurrentEvent.EventParam,CurrentColor);
							if (DestinationType == 0)// 0 -> staging area
							{
								StartingStage = DecipherDestination(CurrentEvent.EventParam,CurrentColor);
								if (StartingStage == 0)  // if none is active, don't drive the motors, re-enter WaitToMove state to keep querying LOC for non-zero active stage
								{
									// Make transition, but stay in WaitToMove state
									MakeTransition = true;
								}
								else // If there is (at least ) one active stage
								{
									NextDrState = MoveToDestination;
									MakeTransition = true;
								}
								printf("StartingStage = %d\r\n",StartingStage);
							}
							else{
							  // if this time of query LOC doesn't return anything (probably go into some exceptional cases), simply request again
								printf("Query again!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
								ES_Timer_InitTimer(ByteTransferIntervalTimer,ByteTransferInterval);
							}
							ReturnEvent.EventType = ES_NO_EVENT; // Wants MasterSM to see NO_EVENT
					 }
         }
				 else // Current Event is now ES_NO_EVENT. Correction 2/20/17 
         {     //Probably means that CurrentEvent was consumed by lower level
            ReturnEvent = CurrentEvent; // in that case update ReturnEvent too.
         }
       break;
			 
			 case MoveToDestination :       // If current state is to move to a shooting spot
				 puts("Current Driving State = MoveToDestination\r\n");
         CurrentEvent = DuringMoveToDestination(CurrentEvent);
         // DuringMoveToShootingSpot excecutes RunMoveToShootingSpot(CurrentEvent)
         if ( CurrentEvent.EventType != ES_NO_EVENT ) //If an event is active
         {
					 if (CurrentEvent.EventType == ES_READY_TO_SHOOT)
					 {
						 	// remap the return event to ES_READY_TO_SHOOT to MasterSM, and transition to MoveToShootingSpot.
						  puts("READY_TO_SHOOT received\r\n");
							ReturnEvent = CurrentEvent; // pass back ES_READY_TO_SHOOT to MasterSM to transition into Shooting
					 }
         }
				 else
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
       RunDrivingSM(CurrentEvent);

       CurrentDrState = NextDrState; //Modify state variable

       //   Execute entry function for new state
       // this defaults to ES_ENTRY
       RunDrivingSM(EntryEventKind);
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
void StartDrivingSM ( ES_Event CurrentEvent )
{
   // to implement entry to a history state or directly to a substate
   // you can modify the initialization of the CurrentDrState variable
   // otherwise just start in the entry state every time the state machine
   // is started
	 
	 // Initialize the color for this game
	 CurrentColor = QueryColor();
   if ( ES_ENTRY_HISTORY != CurrentEvent.EventType )
   {
        CurrentDrState = ENTRY_STATE;
   }
   // call the entry function (if any) for the ENTRY_STATE
   RunDrivingSM(CurrentEvent);
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
DrivingSMState_t QueryDrivingSM ( void )
{
   return(CurrentDrState);
}

/***************************************************************************
 private functions
 ***************************************************************************/

ES_Event DuringWaitToMove( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assme no re-mapping or comsumption

    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
      // No Start functions of lower level SM need to run
			if (isFirstCycle)
			{
				//Call drive function in DCMotorService
				uint8_t Speed = 60;
				if (CurrentColor == RED)
				{
					CurrentDirection = FWD;
					InitialDrive(Speed,FWD);
				}
				else
				{
					CurrentDirection = BWD;
					InitialDrive(Speed,BWD);
				}
				// Start a 1 s timer to move forward and expand the 3 arms
				ES_Timer_InitTimer(ArmsExpansionTimer,ArmsExpansionTime);
			}
			else
			{
				// If it's not the first time driving, directly start a 10 ms ByteTransferTimer before querying LOC for target stage
				ES_Timer_InitTimer(ByteTransferIntervalTimer,ByteTransferInterval);
			}
    }
    else if ( Event.EventType == ES_EXIT )
    {
      // No RunLowerLevelSM(Event) needed;
      // Local exit functionality: None.
      
    } else
    // do the 'during' function for this state
    {
        // No RunLowerLevelSM(Event) needs to run              
    }
    return(ReturnEvent);
}

static ES_Event DuringMoveToDestination( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assme no re-mapping or comsumption
		volatile ES_Event DrLocalEvent = Event;
    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
        // implement any entry actions required for this state machine
        
        // after that start any lower level machines that run in this state
				puts("StartMoveToDestination in the entry function of DrivingSM\r\n");
        StartMoveToDestination( Event );
        // repeat the StartxxxSM() functions for concurrent state machines
        // on the lower level
    }
    else if ( Event.EventType == ES_EXIT )
    {
        // on exit, give the lower levels a chance to clean up first
				puts("RunMoveToDestination(ES_EXIT) in the exit function of DrivingSM\r\n");
        RunMoveToDestination(Event); // should get NO_EVENT back from MovingToSA
        // repeat for any concurrently running state machines
        // now do any local exit functionality
    }else
    // do the 'during' function for this state
    {
        // run any lower level state machine
				ReturnEvent = RunMoveToDestination(Event);
      
        // repeat for any concurrent lower level machines
      
        // do any activity that is repeated as long as we are in this state
    }
    return(ReturnEvent);
}


/******** Function to pass module level static variable to sub machines *****/
uint8_t QueryStartingStage(void)
{
	return StartingStage;
}

