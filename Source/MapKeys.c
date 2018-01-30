/****************************************************************************
 Module
   MapKeys.c

 Revision
   1.0.1

 Description
   This service maps keystrokes to events 

 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 02/06/14 14:44 jec      tweaked to be a more generic key-mapper
 02/07/12 00:00 jec      converted to service for use with E&S Gen2
 02/20/07 21:37 jec      converted to use enumerated type for events
 02/21/05 15:38 jec      Began coding
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
#include <stdio.h>
#include <ctype.h>
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "MapKeys.h"
#include "MasterSM.h"
#include "HallEffectService.h"


/*----------------------------- Module Defines ----------------------------*/

/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this service.They should be functions
   relevant to the behavior of this service
*/


/*---------------------------- Module Variables ---------------------------*/
// with the introduction of Gen2, we need a module level Priority variable
static uint8_t MyPriority;


/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitMapKeys

 Parameters
     uint8_t : the priorty of this service

 Returns
     bool, false if error in initialization, true otherwise

 Description
     Saves away the priority, and does any 
     other required initialization for this service
 Notes

 Author
     J. Edward Carryer, 02/07/12, 00:04
****************************************************************************/
bool InitMapKeys ( uint8_t Priority )
{
  MyPriority = Priority;
	ES_Event ThisEvent;
  ThisEvent.EventType = ES_INIT;
  if (ES_PostToService( MyPriority, ThisEvent) == true)
  {
		puts("MapKeys initialized\r\n");
      return true;
  }else
  {
      return false;
  }
}

/****************************************************************************
 Function
     PostMapKeys

 Parameters
     EF_Event ThisEvent ,the event to post to the queue

 Returns
     bool false if the Enqueue operation failed, true otherwise

 Description
     Posts an event to this state machine's queue
 Notes

 Author
     J. Edward Carryer, 10/23/11, 19:25
****************************************************************************/
bool PostMapKeys( ES_Event ThisEvent )
{
  return ES_PostToService( MyPriority, ThisEvent);
}


/****************************************************************************
 Function
    RunMapKeys

 Parameters
   ES_Event : the event to process

 Returns
   ES_Event, ES_NO_EVENT if no error ES_ERROR otherwise

 Description
   maps keys to Events for HierMuWave Example
 Notes
   
 Author
   J. Edward Carryer, 02/07/12, 00:08
****************************************************************************/
ES_Event RunMapKeys( ES_Event ThisEvent )
{
  ES_Event ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors

    if ( ThisEvent.EventType == ES_NEW_KEY) // there was a key pressed
    {
        switch ( toupper(ThisEvent.EventParam))
        {
          // this sample is just a dummy so it posts a ES_NO_EVENT
						case 'A' : ThisEvent.EventType = ES_LOC_STATUS;
											 break;
						case 'B' : ThisEvent.EventType = ES_BEACON_ALIGNED;
											 puts("Beacon aligned\r\n");
											 break;
						case 'C' : ThisEvent.EventType = ES_SCORE;
											 break;
						case 'H' : ThisEvent.EventType = ES_HANDSHAKE_COMPLETE;
											 break;
						case 'M' : ThisEvent.EventType = ES_MAG_FIELD;
											 break;
						case 'R' : ThisEvent.EventType = ES_LOC_RS;
											 break;
						case 'S' : ThisEvent.EventType = ES_READY_TO_SHOOT;
											 break;
						case 'T' : ThisEvent.EventType = ES_TURN_COMPLETE;
											 break;
						case 'X' : ThisEvent.EventType = ES_X_FOUND;
											 break;
					  case 'D' : ThisEvent.EventParam = ES_NEW_KEY;
        }
        PostMasterSM(ThisEvent);
    }
    
  return ReturnEvent;
}


