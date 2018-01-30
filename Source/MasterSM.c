/****************************************************************************
 Module
   TopHSMTemplate.c

 Revision
   2.0.1

 Description
   This is a template for the top level Hierarchical state machine

 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 02/20/17 14:30 jec      updated to remove sample of consuming an event. We 
                         always want to return ES_NO_EVENT at the top level 
                         unless there is a non-recoverable error at the 
                         framework level
 02/03/16 15:27 jec      updated comments to reflect small changes made in '14 & '15
                         converted unsigned char to bool where appropriate
                         spelling changes on true (was True) to match standard
                         removed local var used for debugger visibility in 'C32
                         removed Microwave specific code and replaced with generic
 02/08/12 01:39 jec      converted from MW_MasterMachine.c
 02/06/12 22:02 jec      converted to Gen 2 Events and Services Framework
 02/13/10 11:54 jec      converted During functions to return Event_t
                         so that they match the template
 02/21/07 17:04 jec      converted to pass Event_t to Start...()
 02/20/07 21:37 jec      converted to use enumerated type for events
 02/21/05 15:03 jec      Began Coding
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
#include "Shooting.h"
#include "MoveToDestination.h"
#include "SPIService.h"
#include "HallEffectService.h"
//#include "BeaconService.h"
#include "DecipherFunctions.h"
#include "LEDService.h"
#include "DCMotorService.h"
#include "ServoGateService.h"
#include "GameTimerModule.h"
#include "DetermineColor.h"
#include "FlywheelTest.h"

/*----------------------------- Module Defines ----------------------------*/
#define ByteTransferInterval 15 //15 ms
#define RED 1
#define GREEN 0
// Identify motor direction
#define FWD 0
#define BWD 1

/*---------------------------- Public Function Prototypes---------------------------*/
bool InitMasterSM(uint8_t Priority);
bool PostMasterSM(ES_Event ThisEvent);
ES_Event RunMasterSM(ES_Event ThisEvent);
void StartMasterSM(ES_Event);

/*---------------------------- Module Functions ---------------------------*/
static ES_Event DuringWaitToStartGame( ES_Event Event);
static ES_Event DuringDrivingSM( ES_Event Event);
static ES_Event DuringShooting( ES_Event Event);

/*---------------------------- Module Variables ---------------------------*/
// everybody needs a state variable, though if the top level state machine
// is just a single state container for orthogonal regions, you could get
// away without it
static MasterState_t CurrentMasterState;
// with the introduction of Gen2, we need a module level Priority var as well
static uint8_t MasterPriority;
//!!! Need to read a pin to determine the color later!!!
static bool Color; // green -> 0, red -> 1
static bool isBackFromDepot;
static bool isFreeShooting;
/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitMasterSM

 Parameters
     uint8_t : the priorty of this service

 Returns
     boolean, False if error in initialization, True otherwise

 Description
     Saves away the priority,  and starts
     the top level state machine
 Notes

 Author
     J. Edward Carryer, 02/06/12, 22:06
****************************************************************************/
bool InitMasterSM ( uint8_t Priority )
{
  ES_Event ThisEvent;

  MasterPriority = Priority;  // save our priority

  ThisEvent.EventType = ES_ENTRY;
  // Start the Master State machine

  StartMasterSM( ThisEvent );
  puts("Init MasterSM\r\n");
  return true;
}

/****************************************************************************
 Function
     PostMasterSM

 Parameters
     ES_Event ThisEvent , the event to post to the queue

 Returns
     boolean False if the post operation failed, True otherwise

 Description
     Posts an event to this state machine's queue
 Notes

 Author
     J. Edward Carryer, 10/23/11, 19:25
****************************************************************************/
bool PostMasterSM( ES_Event ThisEvent )
{
  return ES_PostToService( MasterPriority, ThisEvent);
}

/****************************************************************************
 Function
    RunMasterSM

 Parameters
   ES_Event: the event to process

 Returns
   ES_Event: an event to return

 Description
   the run function for the top level state machine 
 Notes
   uses nested switch/case to implement the machine.
 Author
   J. Edward Carryer, 02/06/12, 22:09
****************************************************************************/
ES_Event RunMasterSM( ES_Event CurrentEvent )
{
   bool MakeTransition = false;/* are we making a state transition? */
   MasterState_t NextMasterState = CurrentMasterState;
   ES_Event EntryEventKind = { ES_ENTRY, 0 };// default to normal entry to new state
   ES_Event ReturnEvent = { ES_NO_EVENT, 0 }; // assume no error
   switch ( CurrentMasterState )
   {
       case WaitToStartGame :       // If current master state is wait to start game
         // Execute During function for WaitToStartGame. ES_ENTRY & ES_EXIT are
         // processed here allow the lower level state machines to re-map
         // or consume the event
				 puts("MasterSM state = WaitToStartGame\r\n");
			   // If MasterSM passes down an entry event, it'll be remapped to NO_EVENT
				 // If that's not an entry event, the during function will not remap and actions will be excecuted here
         CurrentEvent = DuringWaitToStartGame(CurrentEvent);
			   // DuringWaitToStartGame only does querying LOC and
         // changing CurrentEvent to ES_GAME_START when "Construction Active" is received.
         // At the same time it starts an interrupt timer (GameTimer) for 2 minutes, whose ISR stops motors
         // and changes CurrentMasterState back to WaitToStartGame.
			 	 if ( CurrentEvent.EventType != ES_NO_EVENT )
				 {
					 if (CurrentEvent.EventType == ES_TIMEOUT && CurrentEvent.EventParam == ByteTransferIntervalTimer)
					 {
							puts("ByteTransferTimer times out\r\n");
						  ES_Event GSCheckingEvent;
							GSCheckingEvent.EventType = SEND_CMD;
							GSCheckingEvent.EventParam = SB3;
							PostSPIService(GSCheckingEvent);
							puts("Posted Game Status checking request to LOC\r\n");
					 }
					 else if (CurrentEvent.EventType == ES_LOC_STATUS)   // If LOC posts an event about status byte 3 ( byte in its param) 
					 {
						 	static bool GameStart = false;
							GameStart = DecipherGameStatus(CurrentEvent.EventParam);
							if (!GameStart)  // if GameStart == false
							{
								puts("Game has NOT started!\r\n");
								// Re-enter WaitToStartGame so its entry function starts another byte transfer timer
								MakeTransition = true;
							}
							else if (GameStart)
							{
								puts("Game started!\r\n");
								// When GameStart == true, construction is active, ready to transition to Driving
								NextMasterState = Driving;
								MakeTransition = true;
							}
				   }
					 else if (CurrentEvent.EventType == ES_FREE_SHOOTING)  
					 {
							puts("Last 18 seconds!\r\n");
							isFreeShooting = true;
						 	NextMasterState = Driving;
						  MakeTransition = true;
				   }

					 else if (CurrentEvent.EventType == ES_GAME_OVER)    
					 {
							puts("Game over!\r\n");
							// Stop motors
							Stop();
								//!!! Contract arms
								ES_Event ContractLeftArmEvent;
								ContractLeftArmEvent.EventType = ES_CLOSE_LEFT_ROLLERARM;
								PostServoGateService(ContractLeftArmEvent);
								 
								ES_Event ContractRightArmEvent;
								ContractRightArmEvent.EventType = ES_CLOSE_RIGHT_ROLLERARM;
								PostServoGateService(ContractRightArmEvent);
								 
								ES_Event ContractSensorArmEvent;
								ContractSensorArmEvent.EventType = ES_CLOSE_SENSORARM;
								PostServoGateService(ContractSensorArmEvent);
						  // Transition back to WaitToStartGame
						  NextMasterState = WaitToStartGame;
							MakeTransition = true;
				   }
				 }
       break;

			 case Driving :     // If current master state is driving
				 // DuringDrivingSM(Event) is a state machine
			   puts("MasterSM state = Driving\r\n");
				 CurrentEvent = DuringDrivingSM(CurrentEvent);
				 if ( CurrentEvent.EventType != ES_NO_EVENT )
				 {
					 if (CurrentEvent.EventType == ES_READY_TO_SHOOT)
					 {
						 puts("MasterSM state transit from driving to shooting");
						 NextMasterState = Shooting;
						 MakeTransition = true;
					 }
					 else if (CurrentEvent.EventType == ES_FREE_SHOOTING)  
					 {
							puts("Last 18 seconds!!!!!!!!!!!!!!!!!!!!!!!!!!!Now is still driving\r\n");
							isFreeShooting = true;
						 	NextMasterState = Driving;
						  MakeTransition = true;
				   }

					 else if (CurrentEvent.EventType == ES_GAME_OVER)    
					 {
							puts("Game over!\r\n");
							// Stop motors
							Stop();
							//!!! Contract arms
							ES_Event ContractLeftArmEvent;
							ContractLeftArmEvent.EventType = ES_CLOSE_LEFT_ROLLERARM;
							PostServoGateService(ContractLeftArmEvent);
							 
							ES_Event ContractRightArmEvent;
							ContractRightArmEvent.EventType = ES_CLOSE_RIGHT_ROLLERARM;
							PostServoGateService(ContractRightArmEvent);
							 
							ES_Event ContractSensorArmEvent;
							ContractSensorArmEvent.EventType = ES_CLOSE_SENSORARM;
							PostServoGateService(ContractSensorArmEvent);
						  // Transition back to WaitToStartGame
						  NextMasterState = WaitToStartGame;
							MakeTransition = true;
				   }
				 }
				break;
				 
				case Shooting :     // If current master state is shooting
				 // DuringDrivingSM(Event) is a state machine
			   puts("MasterSM state = Shooting\r\n");
				 CurrentEvent = DuringShooting(CurrentEvent); 
				 if ( CurrentEvent.EventType != ES_NO_EVENT )
				 {
					 if (CurrentEvent.EventType == ES_SCORE) // if successfully scored
					 {
						 puts("Notify MasterSM of a successful score (or SupplementCOWs is completed) so that it can transit to Driving state\r\n");
						 // Go back to Driving (which will be initialized to WaitToMove and query LOC for next stage)
						 NextMasterState = Driving;
						 MakeTransition = true;
					 }
					 else if (CurrentEvent.EventType == ES_BACK_FROM_DEPOT) // if successfully scored
					 {
						 puts("Notify MasterSM of returning from the depot\r\n");
						 // Go back to Driving (which will be initialized to WaitToMove and query LOC for next stage)
						 isBackFromDepot = true;
						 NextMasterState = Driving;
						 MakeTransition = true;
					 }
					 // If shooting window is closed
					 else if (CurrentEvent.EventType == ES_SHOOTING_OVER)
					 {
						 puts("20s reached, ready to go to Driving again\r\n");
						 // Go back to Driving (which will be initialized to WaitToMove and query LOC for next stage)
						 NextMasterState = Driving;
						 MakeTransition = true;
					 }
					 else if (CurrentEvent.EventType == ES_FREE_SHOOTING)  
					 {
							puts("Last 18 seconds!!!!!!!!!!!!!!!!!!!!!!!!!!!!Now is shooting\r\n");
							isFreeShooting = true;
							// If it's in shooting state already, no need to move to staging area 1,
						 // just post the event and set the guard true.
				   }

					 else if (CurrentEvent.EventType == ES_GAME_OVER)    
					 {
							puts("Game over!\r\n");
							// Stop motors
							Stop();
							//!!! Contract arms
							ES_Event ContractLeftArmEvent;
							ContractLeftArmEvent.EventType = ES_CLOSE_LEFT_ROLLERARM;
							PostServoGateService(ContractLeftArmEvent);
							 
							ES_Event ContractRightArmEvent;
							ContractRightArmEvent.EventType = ES_CLOSE_RIGHT_ROLLERARM;
							PostServoGateService(ContractRightArmEvent);
							 
							ES_Event ContractSensorArmEvent;
							ContractSensorArmEvent.EventType = ES_CLOSE_SENSORARM;
							PostServoGateService(ContractSensorArmEvent);
						  // Transition back to WaitToStartGame
						  NextMasterState = WaitToStartGame;
							MakeTransition = true;
				   }
				 }
				break;

    }
    //   If we are making a state transition
    if (MakeTransition == true)
    {
       //   Execute exit function for current state
       CurrentEvent.EventType = ES_EXIT;
       RunMasterSM(CurrentEvent);

       CurrentMasterState = NextMasterState; //Modify state variable

       // Execute entry function for new state
       // this defaults to ES_ENTRY
       RunMasterSM(EntryEventKind);
     }
   // in the absence of an error the top level state machine should
   // always return ES_NO_EVENT, which we initialized at the top of func
   return(ReturnEvent);
}
/****************************************************************************
 Function
     StartMasterSM

 Parameters
     ES_Event CurrentEvent

 Returns
     nothing

 Description
     Does any required initialization for this state machine
 Notes

 Author
     J. Edward Carryer, 02/06/12, 22:15
****************************************************************************/
void StartMasterSM ( ES_Event CurrentEvent )
{
	// Initialize peripheral hardware, GPIO functions
  // if there is more than 1 state to the top level machine you will need 
  // to initialize the state variable
  CurrentMasterState = WaitToStartGame;
	Color = DetermineColor();
	ES_Event ColorEvent;
	ColorEvent.EventType = ES_COLORDESIGNATION;
	ColorEvent.EventParam = Color;
	PostLEDService(ColorEvent);
	puts("Color determined\r\n");
	isBackFromDepot = false;
	isFreeShooting = false;
	puts("Posted run flywheel event\r\n");
  // now we need to let the Run function init the lower level state machines
  // use LocalEvent to keep the compiler from complaining about unused var
	puts("Start running RunMasterSM\r\n");
  RunMasterSM(CurrentEvent);
  return;
}


/***************************************************************************
 private functions
 ***************************************************************************/

static ES_Event DuringWaitToStartGame( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assme no re-mapping or comsumption

    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
			ES_Timer_InitTimer(ByteTransferIntervalTimer,ByteTransferInterval);
			puts("Wait 10 ms before querying LOC for Game Status\r\n");
			// No Start functions of lower level SM need to run under WaitToStartGame
    }
    else if ( Event.EventType == ES_EXIT )
    {
			// on exit, give the lower levels a chance to clean up first
      //RunLowerLevelSM(Event);
      // repeat for any concurrently running state machines
      // now do any local exit functionality
      // No RunLowerLevelSM(Event) needed for WaitToStartGame;
			// Start Game timer
			InitGameTimer();
			
    }
		else
    // do the 'during' function for this state
    {
			// Do not remap the event (possibly ES_LOC_RESPONSE.SB3)
			// No RunLowerLevelSM(Event) needs to run under WaitToStartGame            
    }
    return(ReturnEvent);
}

static ES_Event DuringDrivingSM( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assme no re-mapping or comsumption
	
		// For Keil debugger
//		if ( Event.EventType == ES_LOC_RS )
//		{
//			volatile ES_Event Move2SALocalEvent = Event;
//			puts("Dealing with LOC event in DuringDrivingSM under MasterSM\r\n");
//		}
    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
			// implement any entry actions required for this state machine
			// No entry actions for the state of Driving
		
			// after that start any lower level machines that run in this state
			puts("StartDrivingSM in the entry function of DuringDrivingSM\r\n");
			StartDrivingSM( Event );
    }
    else if ( Event.EventType == ES_EXIT )
    {
			// on exit, give the lower levels a chance to clean up first
			puts("RunDrivingSM in the exit function of DuringDrivingSM\r\n");
			RunDrivingSM(Event);
			// No local exit actions
    }
		else
    // do the 'during' function for this state
    {
			// run any lower level state machine
			puts("RunDrivingSM in During function of DuringDrivingSM\r\n");
			ReturnEvent = RunDrivingSM(Event);
			// repeat for any concurrent lower level machines		
			// do any activity that is repeated as long as we are in this state            
    }
    return(ReturnEvent);
}

static ES_Event DuringShooting( ES_Event Event)
{
    ES_Event ReturnEvent = Event; // assme no re-mapping or comsumption

    // process ES_ENTRY, ES_ENTRY_HISTORY & ES_EXIT events
    if ( (Event.EventType == ES_ENTRY) ||
         (Event.EventType == ES_ENTRY_HISTORY) )
    {
			// implement any entry actions required for this state machine
			// No entry actions for the state of Driving
		
			// after that start any lower level machines that run in this state
			puts("StartShooting in the entry function of DuringShootingSM\r\n");
			StartShooting( Event );
    }
    else if ( Event.EventType == ES_EXIT )
    {
			// on exit, give the lower levels a chance to clean up first
			puts("RunShooting in the exit function of DuringShootingSM\r\n");
			RunShooting(Event);
			// No local exit actions
    }
		else
    // do the 'during' function for this state
    {
			// run any lower level state machine
			puts("RunShooting in the During function of DuringShootingSM\r\n");
			ReturnEvent = RunShooting(Event);
			// repeat for any concurrent lower level machines		
			// do any activity that is repeated as long as we are in this state            
    }
    return(ReturnEvent);
}


/*********** Query functions that return module level variables  *******/
bool QueryColor(void)
{
	return Color;
}

bool Query_isBackFromDepot(void)
{
	return isBackFromDepot;
}
bool Query_isFreeShooting(void)
{
	return isFreeShooting;
}
