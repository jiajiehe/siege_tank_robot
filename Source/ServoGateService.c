/***********************************************************************
ServoGateService.c

This service will control shooting cycles (shoot 1 ball or all balls) based on command
***********************************************************************/

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

// the headers to access other services
#include "ServoGateService.h"
#include "ServoPWM.h"
#include "MasterSM.h"

// for choosing servo motors
#define GATE_SERVO 0          //PE5
#define SENSOR_SERVO 1        //PF1
#define LEFT_ROLLER_SERVO 2   //PF2
#define RIGHT_ROLLER_SERVO 3  //PF3


// for ES timers, Assume a 1.000mS/tick timing
#define HALF_SEC 500
#define ONE_SEC 1000
#define QUARTER_SEC 250
#define SINGLE_SHOOT_TIME ONE_SEC
#define FLYWHEEL_PREPARATION_TIME QUARTER_SEC
/*---------------------------- Module Variables ---------------------------*/
// everybody needs a state variable, you may need others as well.
// type of state variable should match htat of enum in header file
static ServoShootingState_t CurrentState;
static uint8_t MyPriority;
static bool HalfOpen;

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitServoGateService

 Parameters
     uint8_t : the priorty of this service

 Returns
     bool, false if error in initialization, true otherwise

 Description
     Saves away the priority, sets up the initial transition and does any
     other required initialization for this state machine
 Notes

 Author
 C. Zhang, 03/03/17, 16:47
****************************************************************************/
bool InitServoGateService ( uint8_t Priority )
{
  ES_Event ThisEvent;
  MyPriority = Priority;
	// GATE SERVO: PE5
	
	// Initialize framework timer resolution (1mS)
	ES_Timer_Init(ES_Timer_RATE_1mS);
	// Intialize PWM channels for PE5, PF1, PF2, PF3
	InitServoPWM();
	// Close servo gate
	CloseServoGate();
	// Initialize all module vars
  CurrentState = Waiting2Open;
	HalfOpen = false;
  // post the initial transition event
  ThisEvent.EventType = ES_INIT;
  if (ES_PostToService( MyPriority, ThisEvent) == true)
  {
      return true;
  }else
  {
      return false;
  }
}

/****************************************************************************
 Function
     PostServoGateService

 Parameters
     EF_Event ThisEvent , the event to post to the queue

 Returns
     boolean False if the Enqueue operation failed, True otherwise

 Description
     Posts an event to this state machine's queue
 Notes

 Author
     C. Zhang, 03/03/17, 16:47
****************************************************************************/
bool PostServoGateService( ES_Event ThisEvent )
{
  return ES_PostToService( MyPriority, ThisEvent);
}

/****************************************************************************
 Function
    RunServoGateService

 Parameters
   ES_Event : the event to process

 Returns
   ES_Event, ES_NO_EVENT if no error ES_ERROR otherwise

 Description
   add your description here
 Notes
   uses nested switch/case to implement the machine.
 Author
   C. Zhang, 03/03/17, 16:47
****************************************************************************/
ES_Event RunServoGateService( ES_Event ThisEvent )
{
  ES_Event ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors

  switch ( CurrentState )
  {
    // In Waiting to Open state
		case Waiting2Open:       
			// If Key is o, which means opening servo gate
		  if (ThisEvent.EventType == ES_OPENGATE){
				// Change CurrentState to Opening
				CurrentState = Opening;
				//printf("Go to shoot 1 cycle\r\n");
				// Turn on flywheel
				//ES_Event Event2Post;
				//Event2Post.EventType = ES_RUNFLYWHEEL;
				//Event2Post.EventParam = TargetRPMVal;
				//PostFlywheelTest(Event2Post);
				// Half Open ServoGate
				HalfOpenServoGate();
				printf("Half open servo gate\r\n");
				// Set HalfOpen to true
				HalfOpen = true;
				// Init ServoGate Timer
				ES_Timer_InitTimer(SERVOGATE_TIMER, FLYWHEEL_PREPARATION_TIME);
			}
    break;
		// In Opening state
		case Opening:
			// If This event is SERVOGATE_TIME timeout
		  if (ThisEvent.EventType == ES_TIMEOUT && ThisEvent.EventParam == SERVOGATE_TIMER){
				// If the gate is half open
				if (HalfOpen == true){
				  // Open ServoGate
				  OpenServoGate();
					//printf("Open servo gate\r\n");
					// Set HalfOpen to false
				  HalfOpen = false;
				  // Init ServoGate Timer
				  ES_Timer_InitTimer(SERVOGATE_TIMER, SINGLE_SHOOT_TIME);
				}
				// Else if the gate is fully open
				else if (HalfOpen == false){
					// Turn off flywheel
					//ES_Event Event2Post;
					//Event2Post.EventType = ES_STOPFLYWHEEL;
					//PostFlywheelTest(Event2Post);
					// Close ServoGate
					CloseServoGate();
					//printf("Close servo gate\r\n");
					// Change CurrentState to Waiting for Command
					CurrentState = Waiting2Open;
					//printf("Shoot 1 cycle ends\r\n");
				}
			}
		break;
	} 
	// Following code is used to control servos for roller arms and sensor arm
	if (ThisEvent.EventType == ES_EXPAND_LEFT_ROLLERARM){
		printf("Expand Left Roller Arm\r\n");
	  ExpandLeftRollerArm();
	}
	if (ThisEvent.EventType == ES_CLOSE_LEFT_ROLLERARM){
		printf("Close Left Roller Arm\r\n");
	  CloseLeftRollerArm();
	}
	if (ThisEvent.EventType == ES_EXPAND_RIGHT_ROLLERARM){
		printf("Expand Right Roller Arm\r\n");
	  ExpandRightRollerArm();
	}
	if (ThisEvent.EventType == ES_CLOSE_RIGHT_ROLLERARM){
		printf("Close Right Roller Arm\r\n");
	  CloseRightRollerArm();
	}
	if (ThisEvent.EventType == ES_EXPAND_SENSORARM){
		printf("Expand Sensor Arm\r\n");
	  ExpandSensorArm();
	}
	if (ThisEvent.EventType == ES_CLOSE_SENSORARM){
		printf("Close Sensor Arm\r\n");
	  CloseSensorArm();
	}
	
	
	
  return ReturnEvent;
}

/***************************************************************************
 private functions
 ***************************************************************************/

