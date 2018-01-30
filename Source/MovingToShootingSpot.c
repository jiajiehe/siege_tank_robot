/****************************************************************************
 Module
   MovingToShootingSpot.c

 Revision
   1.0

 Description
   MoveInX -> TurnToY -> MoveInY (stops when magnetic field detected, then check in) -> Handshake
	 -> if handshake succeeds, TurnToX, get out of MovingToSA and transit to MovingToShootingSpot
	 -> if handshake fails, back to MoveInY

 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 02/28/17 19:35 ZS      Began coding
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
#include "MoveToSA.h" // Need to call QueryShootingLocation()
#include "MoveToShootingSpot.h"
#include "BeaconService.h"
#include "SPIService.h"
#include "DecipherFunctions.h"
#include "DCMotorService.h"

/*----------------------------- Module Defines ----------------------------*/
// define constants for the states for this machine
// and any other local defines

#define ENTRY_STATE S_MoveInY
#define SHOOTING_X 10 // assume shooting lane is aligned with stage 2, which is 10 inches from the wall
// Identify turning direction
#define CW 0
#define CCW 1

/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this machine, things like during
   functions, entry & exit functions.They should be functions relevant to the
   behavior of this state machine
*/

static ES_Event DuringMoveInX( ES_Event Event);
static ES_Event DuringTurnToY( ES_Event Event);
static ES_Event DuringMoveInY( ES_Event Event);
static ES_Event DuringTurnToX( ES_Event Event);

/*---------------------------- Module Variables ---------------------------*/
// Every SM needs a "CurrentState"
static MoveToSSState_t CurrentSSState;
static float CurrentTargetX;  // Initialized in the start function to be SHOOTING_X
static uint8_t CurrentTargetY;
static bool CurrentColor;

/*------------------------------ Module Code ------------------------------*/

/****************************************************************************
 Function
    RunMoveToSA

 Parameters
   ES_Event: the event to process

 Returns
   ES_Event: an event to return

 Description
   A sub-state machine under DrivingSM
 Notes
   uses nested switch/case to implement the machine.
 Author
   J. Edward Carryer, 2/11/05, 10:45AM
****************************************************************************/

ES_Event RunMoveToShootingSpot( ES_Event CurrentEvent )
{
   bool MakeTransition = false;/* are we making a state transition? */
   MoveToSSState_t NextSSState = CurrentSSState;
   ES_Event EntryEventKind = { ES_ENTRY, 0 };// default to normal entry to new state
   ES_Event ReturnEvent = CurrentEvent; // assume no consumption of passed in event
	 
   switch ( CurrentSSState )
   {
       case S_MoveInY :  // If current state is to move in y direction
				 puts("Curent MoveToSA state = MoveInY\r\n");
         CurrentEvent = DuringMoveInY(CurrentEvent);
         // move forward until aligned with the 1450Hz beacon
			 	 if ( CurrentEvent.EventType != ES_NO_EVENT ) //If an event is active
         {
					 if (CurrentEvent.EventType == ES_BEACON_ALIGNED)
					 {
							// Stop motors
						 //!!!
							//StopMotors();
//						 	NextSSState = S_TurnToX; // Next state is to turn to x
//							MakeTransition = true; //mark that we are taking a transition
							ReturnEvent.EventType = ES_READY_TO_SHOOT;  // remap this event to tell DrivingSM it has arrived at the shooting spot
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
       RunMoveToSA(CurrentEvent);

       CurrentSSState = NextSSState; //Modify state variable

       //   Execute entry function for new state
       // this defaults to ES_ENTRY
       RunMoveToSA(EntryEventKind);
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
void StartMoveToShootingSpot ( ES_Event CurrentEvent )
{
   // to implement entry to a history state or directly to a substate
   // you can modify the initialization of the CurrentSSState variable
   // otherwise just start in the entry state every time the state machine
   // is started
	 CurrentTargetX = SHOOTING_X;
	 CurrentColor = QueryColor();
   if ( ES_ENTRY_HISTORY != CurrentEvent.EventType )
   {
        CurrentSSState = ENTRY_STATE;
   }
   // call the entry function (if any) for the ENTRY_STATE
   RunMoveToSA(CurrentEvent);
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
MoveToSSState_t QueryMoveToShootingSpot ( void )
{
   return(CurrentSSState);
}

/***************************************************************************
 private functions
 ***************************************************************************/

/******************1) During functions *********************/

static ES_Event DuringMoveInY( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assume no re-mapping or comsumption
		volatile ES_Event Move2SSLocalEvent = Event;
    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
      // drive along Y
      //!!!
			//DriveForward();
			// Post an event to beacon frequency capture service
			
			// If want to stop in Y by aligning with the beacon, enable the following code
			ES_Event Event2Beacon;
			Event2Beacon.EventType = ES_START_IR_CAPTURE;
			if (CurrentColor) // if current color is red
			{
				Event2Beacon.EventParam = ( TASK_CODE_SHOOTING + COLOR_CODE_RED );
			}
			else
			{
				Event2Beacon.EventParam = ( TASK_CODE_SHOOTING + COLOR_CODE_GREEN);
			}
      PostBeaconService(Event2Beacon);
			puts("Ask the wall IR sensor to start sensing\r\n");
      // if the 1450Hz signal is detected BeaconService should post to MasterSM an ES_BEACON_ALIGNED with freq in its Param

			// If want to stop in Y at a pre-determined distance, enable the following lines
//			// Query DrivingSM for target Y
//			CurrentTargetY = QueryTargetY();
//			//Post an event to motor service
//			ES_Event Event2Motor;
//			Event2Motor.EventType = ES_GO_TARGET_Y;
//			Event2Motor.EventParam = CurrentTargetY;
//      PostDCMotorService(Event2Motor);
//			puts("Ask the motor service to start move to target Y\r\n");
			
    }
    else if ( Event.EventType == ES_EXIT )
    {
      // No RunLowerLevelSM(Event) needed;
      // Local exit functionality: None.
      
    }else
    // do the 'during' function for this state
    {
      // No RunLowerLevelSM(Event) needs to run
      // When ES_BEACON_ALIGNED is posted to MasterSM, DuringMoveInY() goes here.
      // Want ReturnEvent to remain as the same so no remapping.
    }
    return(ReturnEvent);
}

