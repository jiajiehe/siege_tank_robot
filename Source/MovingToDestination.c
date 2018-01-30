 /****************************************************************************
 Module
   MovingToSA.c

 Revision
   1.0

 Description

 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 02/28/17 19:21 ZS      Updated TurnToX
 03/04/17 14:23 ZS			For straight forward/backward moving strategy, bypass MoveInX, TurnToY, TurnToY.
												And change the MoveToStage stop criterium to ES_TARGET_Y_HIT which will be posted by
												the motor service.
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
#include "HallEffectService.h"
#include "SPIService.h"
#include "DecipherFunctions.h"
#include "DCMotorService.h"
//#include "BeaconService.h"
#include "UltrasonicTest.h"

/*----------------------------- Module Defines ----------------------------*/
// define constants for the states for this machine
// and any other local defines

#define ENTRY_STATE MoveToStage 
#define REPORT_INTERVAL 300 // 300 ms between consecutive query of LOC report status
// Identify motor direction
#define FWD 0
#define BWD 1
#define MagDelayTime 1000  // 1 s

/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this machine, things like during
   functions, entry & exit functions.They should be functions relevant to the
   behavior of this state machine
*/
static void HitWall(void);
static ES_Event DuringMoveToStage( ES_Event Event);
static ES_Event DuringCheckIn( ES_Event Event);
static ES_Event DuringHandshake( ES_Event Event);
static ES_Event DuringGoToShootingSpot( ES_Event Event);

/*---------------------------- Module Variables ---------------------------*/
// Every SM needs a "CurrentState"
static MoveToDestinationState_t CurrentMTDstate;
static bool AckStatus;
static uint8_t TargetStage;  // TargetStage is updated every time a new cycle starts (when StartMovingToDestination() gets called)
static uint8_t TargetShootingLocation;
static bool CurrentColor;
static uint8_t CurrentLocation = BACK_WALL;  // CurrentLocation initializes to BACK_WALL
																						 // And it will remember the current location as it's static
																						 // All other state machines query CurrentLocation from here if needed
static uint8_t Speed = 80;  //rpm
static bool CurrentDirection;
static uint8_t HitWallCounter = 0;
/*------------------------------ Module Code ------------------------------*/

/****************************************************************************
 Function
    RunMoveToDestination

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

ES_Event RunMoveToDestination( ES_Event CurrentEvent )
{
   bool MakeTransition = false;/* are we making a state transition? */
   MoveToDestinationState_t NextMTDstate = CurrentMTDstate;
   ES_Event EntryEventKind = { ES_ENTRY, 0 };// default to normal entry to new state
   ES_Event ReturnEvent = CurrentEvent; // assume no consumption of passed in event

   switch ( CurrentMTDstate )
   {

       case MoveToStage :  // If current state is to move in y direction
				 puts("Current MoveToDestination state = MoveToStage\r\n");
         CurrentEvent = DuringMoveToStage(CurrentEvent);
         // move forward until a mag field (among the list) is detected
         // and the event will be posted ES_MAG_FIELD, with the frequency as the EventParam
			 	 if ( CurrentEvent.EventType != ES_NO_EVENT ) //If an event is active
         {
					 if (CurrentEvent.EventType == ES_TIMEOUT && CurrentEvent.EventParam == MagDelayTimer)
					 {
						 	// Post an event to magnetic field frequency capture service
							ES_Event Event2HallEffect;
							Event2HallEffect.EventType = ES_START_MAG_FIELD_CAPTURE;
							if (TargetStage == 1 || TargetStage == 3)
							{
								// enable the Hall effect sensor on the left if target stage is 1 or 3
								Event2HallEffect.EventParam = LEFT;
								puts("Enable left-side sensor\r\n");
							}
							else
							{
								// enable the Hall effect sensor on the right if target stage is 2
								Event2HallEffect.EventParam = RIGHT;
								puts("Enable right-side sensor\r\n");
							}
							PostHallEffectService(Event2HallEffect);
							puts("Ask the Hall Effect sensor to start\r\n");
							NextMTDstate = CheckIn;
							MakeTransition = true;
							ReturnEvent.EventType = ES_NO_EVENT; // Consume this ES_MAG_FIELD event
					} else if (CurrentEvent.EventType == ES_MOTOR_STALL)
					{
						 HitWall();
						 // Re-enter the MoveToStage state with updated location
						 NextMTDstate = MoveToStage;
						 MakeTransition = true;
						 ReturnEvent.EventType = ES_NO_EVENT;  // consume this event and pass NO_EVENT to DrivingSM
					}
				} else
        {     //Probably means that CurrentEvent was consumed by lower level
            ReturnEvent = CurrentEvent; // in that case update ReturnEvent too.
        }
       break;
			 
			 case CheckIn:
				 puts("Current MoveToDestination state = CheckIn\r\n");
         CurrentEvent = DuringCheckIn(CurrentEvent);
         // move forward until a mag field (among the list) is detected
         // and the event will be posted ES_MAG_FIELD, with the frequency as the EventParam
			 	 if ( CurrentEvent.EventType != ES_NO_EVENT ) //If an event is active
         {
				 	 if (CurrentEvent.EventType == ES_MAG_FIELD)  // If want to stop when a target distance is hit, EventType should be ES_TARGET_Y_HIT
					 {
							// Motors must have been stopped
						  Stop();
							// Post an event to LOC service to report mag field freq.
						  printf("Captured mag field HI period time =%d\r\n",CurrentEvent.EventParam);
						  ES_Event ReportEvent;
							ReportEvent.EventType = SEND_CMD;
							ReportEvent.EventParam = REPORT;
						  PostSPIService(ReportEvent);
							ReturnEvent.EventType = ES_NO_EVENT; // Consume this ES_MAG_FIELD event
							// Stay in MoveToStage and wait for LOC to get back
					 }
					 else if (CurrentEvent.EventType == ES_LOC_RS) // if LOC posts an event about the report status
					 {
						  // DecipherReportStatus() reads bit 7 and 6
						  // 00-ACK, 10-inactive, 11-NACK
						  // returns true for ACK, false for the other two
							AckStatus = DecipherReportStatus(CurrentEvent.EventParam);
						  // DecipherLocationInReport() returns the number of current staging area (for our color)
							CurrentLocation = DecipherLocationInReport(CurrentEvent.EventParam);
						  printf("CurrentLocation = %d (1:stage1, 2:stage2, 3:stage3 regardless of the color)\r\n",CurrentLocation);
							if ( AckStatus ) // if LOC returns ACK
							{
								// The reported freq is valid, continue to complete the handshake
								// (handshake = query again with the newly detected freq for shooting area)
								NextMTDstate = Handshake;
								MakeTransition = true; //mark that we are taking a transition
								puts("ACK\r\n");
							}
							else // if LOC returns NACK or inactive
							{
								// If check-in fails, re-enter state MoveToStage. Execute again the entry function
								// (continue MoveToStage in Y to find next stage)
								puts("NACK\r\n");
								CurrentLocation = DecipherLocationInReport(CurrentEvent.EventParam);
								NextMTDstate = MoveToStage;
								MakeTransition = true;
							}
							ReturnEvent.EventType = ES_NO_EVENT;  // consume this event and pass NO_EVENT to DrivingSM
					 }
					 else if (CurrentEvent.EventType == ES_MOTOR_STALL)
					 {
						 HitWall();
						 // Re-enter the MoveToStage state with updated location
						 NextMTDstate = MoveToStage;
						 MakeTransition = true;
						 ReturnEvent.EventType = ES_NO_EVENT;  // consume this event and pass NO_EVENT to DrivingSM
					 }
         }
				 else
         {     //Probably means that CurrentEvent was consumed by lower level
            ReturnEvent = CurrentEvent; // in that case update ReturnEvent too.
         }
				 break;

       case Handshake :  // If current state is to get the shooting area code
				 puts("Current MoveToDestination state = Handshake\r\n");
         CurrentEvent = DuringHandshake(CurrentEvent);
         // DuringHandshake reports the mag field frequency to LOC again
         // in exchange for the next active shooting area
         // An event of type ES_MAG_FIELD will be posted from MagFieldCapture with the new freq
			 	 if ( CurrentEvent.EventType != ES_NO_EVENT ) //If an event is active
         {
					 if (CurrentEvent.EventType == ES_TIMEOUT && CurrentEvent.EventParam == ReportGap)
					 {
						  puts("300ms passed, initiate handshake now\r\n");
						 	// Post an event to magnetic field frequency capture service
							ES_Event Event2MagFieldService;
							Event2MagFieldService.EventType = ES_START_MAG_FIELD_CAPTURE;
						  if (TargetStage == 1 || TargetStage == 3)
							{
								Event2MagFieldService.EventParam = LEFT;
							}
							else
							{
								Event2MagFieldService.EventParam = RIGHT;
							}
							PostHallEffectService(Event2MagFieldService);
							ReturnEvent.EventType = ES_NO_EVENT;
							// if MagField is detected MagFieldService should post to MasterSM an ES_MAG_FIELD with freq in its Param
					 }
					 
					 else if (CurrentEvent.EventType == ES_MAG_FIELD)
					 {
						 	// Post an event to LOC service to report mag field freq.
						  puts("Magnetic field detected. Shake hand with LOC\r\n");
						  ES_Event ReportEvent;
							ReportEvent.EventType = SEND_CMD;
							ReportEvent.EventParam = REPORT;
						  PostSPIService(ReportEvent);
							ReturnEvent.EventType = ES_NO_EVENT; // Consume this ES_MAG_FIELD event
							// Stay in Handshake and wait for LOC to get back
					 }
					 else if (CurrentEvent.EventType == ES_LOC_RS)
					 {
						  // DecipherReportStatus() reads bit 7 and 6
						  // 00-ACK, 10-inactive, 11-NACK
						  // returns true for ACK, false for the other two
							AckStatus = DecipherReportStatus(CurrentEvent.EventParam);
							if ( AckStatus ) // if LOC returns ACK
							{
								// The reported freq is valid, handshake completed
								// Store the shooting area code
								// DecipherLocationInReport() returns the number of current shooting area
								TargetShootingLocation = DecipherLocationInReport(CurrentEvent.EventParam);
								printf("Shooting area %d opened \r\n",TargetShootingLocation);
								NextMTDstate = GoToShootingSpot;
								MakeTransition = true; //mark that we are taking a transition
							}
							else // if LOC returns NACK or inactive
							{
								puts("Handshake failed\r\n");
								// Update CurrentStage
								CurrentLocation = DecipherLocationInReport(CurrentEvent.EventParam);
								NextMTDstate = MoveToStage; // move to target stage and restart the check-in process
								MakeTransition = true;  // consume this event and pass NO_EVENT to DrivingSM
							}
							ReturnEvent.EventType = ES_NO_EVENT;
						}
         }
				 else
         {     //Probably means that CurrentEvent was consumed by lower level
            ReturnEvent = CurrentEvent; // in that case update ReturnEvent too.
         }
       break;
				 
			 case GoToShootingSpot:  // If current state is to move in y direction
				 puts("Current MoveToDestination state = GoToShootingSpot\r\n");
         CurrentEvent = DuringGoToShootingSpot(CurrentEvent);
			 	 if ( CurrentEvent.EventType != ES_NO_EVENT ) //If an event is active
         {
					 // Only when we need to shoot into bucket 2 that we would go into this block of code
					 // Need to stop when a certain distance to the wall is detected
					 if ( CurrentEvent.EventType == ES_DISTANCE_DETECTED)
					 {
							 // Stop motors
							 Stop();
						  // Update CurrentLocation
						  CurrentLocation = TargetShootingLocation;
							ReturnEvent.EventType = ES_NO_EVENT;  // CONSUME this event
						  // Post notification event to MasterSM for it to transit from Driving to Shooting
							ES_Event ThisEvent;
							ThisEvent.EventType = ES_READY_TO_SHOOT;
							PostMasterSM(ThisEvent);
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
       RunMoveToDestination(CurrentEvent);

       CurrentMTDstate = NextMTDstate; //Modify state variable

       //   Execute entry function for new state
       // this defaults to ES_ENTRY
       RunMoveToDestination(EntryEventKind);
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
void StartMoveToDestination ( ES_Event CurrentEvent )
{
   // to implement entry to a history state or directly to a substate
   // you can modify the initialization of the CurrentMTDstate variable
   // otherwise just start in the entry state every time the state machine
   // is started
	 CurrentColor = QueryColor();  // 0-> green, 1-> red
	 bool LastDepot, FreeShooting;
	 LastDepot = Query_isBackFromDepot();
	 FreeShooting = Query_isFreeShooting();
	 if (LastDepot)
	 {
		 // If just returned from the supplu depot, initialize the currentlocation to DEPOT
		 CurrentLocation = DEPOT;
	 }
	 if (FreeShooting)
	 {
		 TargetStage = SA_1;  // in the free shooting period, always go to stage #1
	 }
	 else
	 {
		 TargetStage = QueryStartingStage(); // in normal cases, query for starting stage and update the current destination
	 }
   if ( ES_ENTRY_HISTORY != CurrentEvent.EventType )
   {
        CurrentMTDstate = ENTRY_STATE;
   }
   // call the entry function (if any) for the ENTRY_STATE
   RunMoveToDestination(CurrentEvent);
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
MoveToDestinationState_t QueryCurrentMTDstate ( void )
{
   return(CurrentMTDstate);
}

/****************************************************************************
 Function
     QueryTargetShootingLocation

 Parameters
     None

 Returns
     TargetShootingLocation (the number of the shooting goal)

 Description
     returns which shooting goal is open
 Notes

 Author
Zoey, 2/28/17, 19:41
****************************************************************************/


/***************************************************************************
 private functions
 ***************************************************************************/


static void HitWall(void) {
	 Stop();
	 if (CurrentColor == GREEN)
	 {
		 if (CurrentDirection == BWD)
		 {
		 puts("Hit the wall at depot\r\n");
		 CurrentLocation = DEPOT;
		 } else
		 {
		 puts("Hit the back wall\r\n");
		 CurrentLocation = BACK_WALL;
		 }
	 }
	 else if (CurrentColor == RED) {
		 if (CurrentDirection == FWD)
		 {
		 puts("Hit the wall at depot\r\n");
		 CurrentLocation = DEPOT;
		 } else
		 {
		 puts("Hit the back wall\r\n");
		 CurrentLocation = BACK_WALL;
		 }
	 }
}
/******************1) During functions *********************/


static ES_Event DuringMoveToStage( ES_Event Event)
{
	puts("Enter DuringMoveToStage\r\n");
    ES_Event ReturnEvent = Event; // assume no re-mapping or comsumption
		ES_Timer_InitTimer(MagDelayTimer,MagDelayTime);
    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
			// Determine driving direction based on Color, CurrentLocation, and TargetStage
			if (CurrentColor == RED)
			{
				if (CurrentLocation == BACK_WALL)
				{
					//Call drive function in DCMotorService
					CurrentDirection = FWD;
					Drive(Speed,FWD);
				}
				else if (CurrentLocation - TargetStage > 0)
				{
					//Call drive function in DCMotorService
					CurrentDirection = FWD;
					Drive(Speed,FWD);
				}
				else if (CurrentLocation - TargetStage < 0)
				{
					//Call drive function in DCMotorService
					CurrentDirection = BWD;
					Drive(Speed,BWD);
				}
				// else if CurrentLocation == TargetStage, don't need to drive the motors
			}
			
			else if (CurrentColor == GREEN)
			{
				if (CurrentLocation == BACK_WALL)
				{
					//Call drive function in DCMotorService
					CurrentDirection = BWD;
					Drive(Speed,BWD);
				}
				else if (CurrentLocation - TargetStage > 0)
				{
					//Call drive function in DCMotorService
					CurrentDirection = BWD;
					Drive(Speed,BWD);
				}
				else if (CurrentLocation - TargetStage < 0)
				{
					//Call drive function in DCMotorService
					CurrentDirection = FWD;
					Drive(Speed,FWD);
				}
				// else if CurrentLocation == TargetStage, don't need to drive the motors
			}
			
    }
    else if ( Event.EventType == ES_EXIT )
    {
      // No RunLowerLevelSM(Event) needed;
      // Local exit functionality: None.
      
    }else
    // do the 'during' function for this state
    {
      // No RunLowerLevelSM(Event) needs to run
      // When ES_MAG_FIELD is posted by the MagFieldFreq(), or ES_LOC_RS is posted by LOC, DuringMoveToStage() goes here.
      // Want ReturnEvent to remain as ES_MAG_FIELD or ES_LOC_RS so no remapping.
    }
    return(ReturnEvent);
}

static ES_Event DuringCheckIn( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assume no re-mapping or comsumption
		ES_Timer_InitTimer(MagDelayTimer,MagDelayTime);
    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
				puts("Enter DuringCheckIn\r\n");			
    }
    else if ( Event.EventType == ES_EXIT )
    {
      // No RunLowerLevelSM(Event) needed;
      // Local exit functionality: None.
      
    }else
    // do the 'during' function for this state
    {
      // No RunLowerLevelSM(Event) needs to run
      // When ES_MAG_FIELD is posted by the MagFieldFreq(), or ES_LOC_RS is posted by LOC, DuringMoveToStage() goes here.
      // Want ReturnEvent to remain as ES_MAG_FIELD or ES_LOC_RS so no remapping.
    }
    return(ReturnEvent);
}

static ES_Event DuringHandshake( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assume no re-mapping or comsumption
		volatile ES_Event Move2SALocalEvent = Event;
    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
			// Start a timer called ReportGap to allow at least 200 ms from last report
			puts("Init a 300 ms timer\r\n");
			ES_Timer_InitTimer(ReportGap, REPORT_INTERVAL);
    }
    else if ( Event.EventType == ES_EXIT )
    {
      // No RunLowerLevelSM(Event) needed;
      // Local exit functionality: None.
			puts("Excecuted exit function of MoveToDestination's state: Handshake\r\n");
      
    }else
    // do the 'during' function for this state
    {
      // No RunLowerLevelSM(Event) needs to run
      // When ES_MAG_FIELD is posted by the MagFieldFreq(), or ES_LOC_RS is posted by LOC, DuringHandshake() goes here.
      // Want ReturnEvent to remain as ES_MAG_FIELD or ES_LOC_RS so no remapping.
    }
    return(ReturnEvent);
}

static ES_Event DuringGoToShootingSpot( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assume no re-mapping or comsumption
		volatile ES_Event Move2SALocalEvent = Event;
    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
			// Determine driving direction based on Color, CurrentLocation, and TargetStage
			if (CurrentColor == RED)
			{
				if (CurrentLocation - TargetShootingLocation > 0)
				{
					//Call drive function in DCMotorService
					CurrentDirection = FWD;
					Drive(Speed,FWD);
				}
				else if (CurrentLocation - TargetShootingLocation < 0)
				{
					//Call drive function in DCMotorService
					CurrentDirection = BWD;
					Drive(Speed,BWD);
				}
				// if robot is on SA2 now but need to shoot to bucket 2
				if (CurrentLocation == 2)
				{ 
					// then simply drive backward. it would be stopped by ultrasonic sensor
					CurrentDirection = BWD;
					Drive(Speed,BWD);
				}
				// else if CurrentLocation == TargetShootingLocation, don't need to drive the motors
			}
			
			else if (CurrentColor == GREEN)
			{
				if (CurrentLocation - TargetShootingLocation > 0)
				{
					//Call drive function in DCMotorService
					CurrentDirection = BWD;
					Drive(Speed,BWD);
				}
				else if (CurrentLocation - TargetShootingLocation < 0)
				{
					//Call drive function in DCMotorService
					CurrentDirection = FWD;
					Drive(Speed,FWD);
				}
				// else if CurrentLocation == TargetShootingLocation, don't need to drive the motors
			}
			
			
			  // IF THE TARGET SHOOTING LOCATION IS 2, post an event to Ultrasonic service
			if (TargetShootingLocation == 2)
			{
				if (CurrentLocation != TargetShootingLocation)
				{
					ES_Event Event2Ultrasonic;
					Event2Ultrasonic.EventType = ES_START_ULTRASONIC;
					PostUltrasonicTest(Event2Ultrasonic);
					puts("Now detect if the distance to the wall has reached 600-700mm or not\r\n");
					/*
					ES_Event Event2Beacon;
					Event2Beacon.EventType = ES_START_IR_CAPTURE;
					PostBeaconService(Event2Beacon);
					*/
					// if the 1450Hz signal is detected BeaconService should post to MasterSM an ES_BEACON_ALIGNED with freq in its Param
				}
				else
				{
					ES_Event Event2Ultrasonic;
					Event2Ultrasonic.EventType = ES_START_ULTRASONIC;
					PostUltrasonicTest(Event2Ultrasonic);
					puts("Now detect if the distance to the wall has reached 600-700mm or not\r\n");
					/*
					ES_Event Event2Beacon;
					Event2Beacon.EventType = ES_START_IR_CAPTURE;
					PostBeaconService(Event2Beacon);
					*/
					// if the 1450Hz signal is detected BeaconService should post to MasterSM an ES_BEACON_ALIGNED with freq in its Param
					/*
					// if CurrentLocation is already the TargetShootingLocation, post an event ES_READY_TO_SHOOT to MasterSM
					ES_Event ThisEvent;
					ThisEvent.EventType = ES_READY_TO_SHOOT;
					PostMasterSM(ThisEvent);
					*/
				}
			}
			else if (TargetShootingLocation == 1 || TargetShootingLocation == 3 || TargetShootingLocation == 4)
				// IF THE TARGET SHOOTING LOCATION IS 1 or 3, post an event to Hall Effect service
			{
				if (CurrentLocation != TargetShootingLocation)
				{
					// Possible scenarios: 1 -> 3, 2->3, 3->1, 2->1. In all cases, and regardless of the color,
					// always turn on the left Hall Effect sensor so that the next mag field must be the TargetShootingLocation
					ES_Event Event2HallEffect;
					Event2HallEffect.EventType = ES_START_MAG_FIELD_CAPTURE;
					Event2HallEffect.EventParam = LEFT;
					PostHallEffectService(Event2HallEffect);
					puts("Ask the 1/3 Hall Effect sensor to start\r\n");
				}
				else
				{
					// if CurrentLocation is already the TargetShootingLocation, post an event ES_READY_TO_SHOOT to MasterSM
					ES_Event ThisEvent;
					ThisEvent.EventType = ES_READY_TO_SHOOT;
					PostMasterSM(ThisEvent);
				}

				// if MagField is detected MagFieldService should post to MasterSM an ES_MAG_FIELD with freq in its Param
			}			
    }
    else if ( Event.EventType == ES_EXIT )
    {
      // No RunLowerLevelSM(Event) needed;
      // Local exit functionality: None.
			puts("Excecuted exit function of MoveToDestination's BUFFER state (SuccessfulHandshake)\r\n");
      
    }else
    // do the 'during' function for this state
    {
      // No RunLowerLevelSM(Event) needs to run
      // When ES_MAG_FIELD is posted by the MagFieldFreq(), or ES_LOC_RS is posted by LOC, DuringHandshake() goes here.
      // Want ReturnEvent to remain as ES_MAG_FIELD or ES_LOC_RS so no remapping.
    }
    return(ReturnEvent);
}

/******** Function to pass module level static variable to sub machines *****/

uint8_t QueryCurrentLocation(void)
{
	return (CurrentLocation);
}
uint8_t QueryTargetShootingLocation(void)
{
	return TargetShootingLocation;
}	
