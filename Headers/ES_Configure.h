/****************************************************************************
 Module
     ES_Configure.h
 Description
     This file contains macro definitions that are edited by the user to
     adapt the Events and Services framework to a particular application.
 Notes
     
 History
 When           Who     What/Why
 -------------- ---     --------
 10/21/13 20:54 jec      lots of added entries to bring the number of timers
                         and services up to 16 each
 08/06/13 14:10 jec      removed PostKeyFunc stuff since we are moving that
                         functionality out of the framework and putting it
                         explicitly into the event checking functions
 01/15/12 10:03 jec      started coding
*****************************************************************************/

#ifndef CONFIGURE_H
#define CONFIGURE_H

/****************************************************************************/
// The maximum number of services sets an upper bound on the number of 
// services that the framework will handle. Reasonable values are 8 and 16
// corresponding to an 8-bit(uint8_t) and 16-bit(uint16_t) Ready variable size
#define MAX_NUM_SERVICES 16

/****************************************************************************/
// This macro determines that nuber of services that are *actually* used in
// a particular application. It will vary in value from 1 to MAX_NUM_SERVICES
#define NUM_SERVICES 9

/****************************************************************************/
// These are the definitions for Service 0, the lowest priority service.
// Every Events and Services application must have a Service 0. Further 
// services are added in numeric sequence (1,2,3,...) with increasing 
// priorities
// the header file with the public function prototypes
#define SERV_0_HEADER "SPIService.h"
// the name of the Init function
#define SERV_0_INIT InitSPIService
// the name of the run function
#define SERV_0_RUN RunSPIService
// How big should this services Queue be?
#define SERV_0_QUEUE_SIZE 3

/****************************************************************************/
// The following sections are used to define the parameters for each of the
// services. You only need to fill out as many as the number of services 
// defined by NUM_SERVICES
/****************************************************************************/
// These are the definitions for Service 1
#if NUM_SERVICES > 1
// the header file with the public function prototypes
#define SERV_1_HEADER "LEDService.h"
// the name of the Init function
#define SERV_1_INIT InitLEDService
// the name of the run function
#define SERV_1_RUN RunLEDService
// How big should this services Queue be?
#define SERV_1_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 2
#if NUM_SERVICES > 2
// the header file with the public function prototypes
#define SERV_2_HEADER "HallEffectService.h"
// the name of the Init function
#define SERV_2_INIT InitHallEffectService
// the name of the run function
#define SERV_2_RUN RunHallEffectService
// How big should this services Queue be?
#define SERV_2_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 3
#if NUM_SERVICES > 3
// the header file with the public function prototypes
#define SERV_3_HEADER "DCMotorService.h"
// the name of the Init function
#define SERV_3_INIT InitDCMotorService
// the name of the run function
#define SERV_3_RUN RunDCMotorService
// How big should this services Queue be?
#define SERV_3_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 4
#if NUM_SERVICES > 4
// the header file with the public function prototypes
#define SERV_4_HEADER "UltrasonicTest.h"
// the name of the Init function
#define SERV_4_INIT InitUltrasonicTest
// the name of the run function
#define SERV_4_RUN RunUltrasonicTest
// How big should this services Queue be?
#define SERV_4_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 5
#if NUM_SERVICES > 5
// the header file with the public function prototypes
#define SERV_5_HEADER "COWSupplementService.h"
// the name of the Init function
#define SERV_5_INIT InitCOWSupplementService
// the name of the run function
#define SERV_5_RUN RunCOWSupplementService
// How big should this services Queue be?
#define SERV_5_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 6
#if NUM_SERVICES > 6
// the header file with the public function prototypes
#define SERV_6_HEADER "FlywheelTest.h"
// the name of the Init function
#define SERV_6_INIT InitFlywheelTest
// the name of the run function
#define SERV_6_RUN RunFlywheelTest
// How big should this services Queue be?
#define SERV_6_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 7
#if NUM_SERVICES > 7
// the header file with the public function prototypes
#define SERV_7_HEADER "ServoGateService.h"
// the name of the Init function
#define SERV_7_INIT InitServoGateService
// the name of the run function
#define SERV_7_RUN RunServoGateService
// How big should this services Queue be?
#define SERV_7_QUEUE_SIZE 3
#endif

/*****************************************************************************/
// These are the definitions for Service 8
#if NUM_SERVICES > 8
// the header file with the public function prototypes
#define SERV_8_HEADER "MasterSM.h"
// the name of the Init function
#define SERV_8_INIT InitMasterSM
// the name of the run function
#define SERV_8_RUN RunMasterSM
// How big should this services Queue be?
#define SERV_8_QUEUE_SIZE 3
#endif
/****************************************************************************/
// These are the definitions for Service 9
#if NUM_SERVICES > 9
// the header file with the public function prototypes
#define SERV_9_HEADER "MasterSM.h"
// the name of the Init function
#define SERV_9_INIT InitMasterSM
// the name of the run function
#define SERV_9_RUN RunMasterSM
// How big should this services Queue be?
#define SERV_9_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 10
#if NUM_SERVICES > 10
// the header file with the public function prototypes
#define SERV_10_HEADER "TestHarnessService10.h"
// the name of the Init function
#define SERV_10_INIT InitTestHarnessService10
// the name of the run function
#define SERV_10_RUN RunTestHarnessService10
// How big should this services Queue be?
#define SERV_10_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 11
#if NUM_SERVICES > 11
// the header file with the public function prototypes
#define SERV_11_HEADER "TestHarnessService11.h"
// the name of the Init function
#define SERV_11_INIT InitTestHarnessService11
// the name of the run function
#define SERV_11_RUN RunTestHarnessService11
// How big should this services Queue be?
#define SERV_11_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 12
#if NUM_SERVICES > 12
// the header file with the public function prototypes
#define SERV_12_HEADER "TestHarnessService12.h"
// the name of the Init function
#define SERV_12_INIT InitTestHarnessService12
// the name of the run function
#define SERV_12_RUN RunTestHarnessService12
// How big should this services Queue be?
#define SERV_12_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 13
#if NUM_SERVICES > 13
// the header file with the public function prototypes
#define SERV_13_HEADER "TestHarnessService13.h"
// the name of the Init function
#define SERV_13_INIT InitTestHarnessService13
// the name of the run function
#define SERV_13_RUN RunTestHarnessService13
// How big should this services Queue be?
#define SERV_13_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 14
#if NUM_SERVICES > 14
// the header file with the public function prototypes
#define SERV_14_HEADER "TestHarnessService14.h"
// the name of the Init function
#define SERV_14_INIT InitTestHarnessService14
// the name of the run function
#define SERV_14_RUN RunTestHarnessService14
// How big should this services Queue be?
#define SERV_14_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 15
#if NUM_SERVICES > 15
// the header file with the public function prototypes
#define SERV_15_HEADER "TestHarnessService15.h"
// the name of the Init function
#define SERV_15_INIT InitTestHarnessService15
// the name of the run function
#define SERV_15_RUN RunTestHarnessService15
// How big should this services Queue be?
#define SERV_15_QUEUE_SIZE 3
#endif


/****************************************************************************/
// Name/define the events of interest
// Universal events occupy the lowest entries, followed by user-defined events
typedef enum {  ES_NO_EVENT = 0,
                ES_ERROR,  /* used to indicate an error from the service */
                ES_INIT,   /* used to transition from initial pseudo-state */
                ES_NEW_KEY, /* signals a new key received from terminal */
                ES_TIMEOUT, /* signals that the timer has expired */
                ES_ENTRY,
                ES_ENTRY_HISTORY,
                ES_EXIT,
                /* User-defined events start here */
                ES_LOC_STATUS,
								ES_LOC_RS,
								SEND_CMD,
	              SSI_EOT,
								ES_HANDSHAKE_COMPLETE,
								ES_MAG_FIELD,
								ES_TURN_NINETY,
								ES_TURN_COMPLETE,
								ES_X_FOUND,
								ES_GO_TARGET_Y,
								ES_TARGET_Y_HIT,
								ES_START_MAG_FIELD_CAPTURE,
								ES_MAGNETIC_PULSE,
								ES_READY_TO_SHOOT,
								ES_SHOOTING_OVER,
								ES_START_IR_CAPTURE,
								ES_IR_PULSE,
								ES_BEACON_ALIGNED,
								ES_SCORE,
								ES_RELOAD,
								ES_FULL_LOAD,
								ES_PULSE_DONE,
								ES_RUNFLYWHEEL,
								ES_STOPFLYWHEEL,
								ES_COLORDESIGNATION,
								ES_QUERYBALL,
								ES_OPENGATE,
								ES_EXPAND_LEFT_ROLLERARM,
								ES_CLOSE_LEFT_ROLLERARM,
								ES_EXPAND_RIGHT_ROLLERARM,
								ES_CLOSE_RIGHT_ROLLERARM,
								ES_EXPAND_SENSORARM,
								ES_CLOSE_SENSORARM,
								ES_MOTOR_STALL,
								ES_BACK_FROM_DEPOT,
								ES_FREE_SHOOTING,
								ES_GAME_OVER,
								ES_ULTRASONIC_CAPTURE,
								ES_DISTANCE_DETECTED,
								ES_START_ULTRASONIC
								} ES_EventTyp_t ;

/****************************************************************************/
// These are the definitions for the Distribution lists. Each definition
// should be a comma separated list of post functions to indicate which
// services are on that distribution list.
#define NUM_DIST_LISTS 1
#if NUM_DIST_LISTS > 0 
#define DIST_LIST0 PostMasterSM
#endif
#if NUM_DIST_LISTS > 1 
#define DIST_LIST1 PostTemplateFSM, TemplateFSM
#endif
#if NUM_DIST_LISTS > 2 
#define DIST_LIST2 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 3 
#define DIST_LIST3 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 4 
#define DIST_LIST4 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 5 
#define DIST_LIST5 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 6 
#define DIST_LIST6 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 7 
#define DIST_LIST7 PostTemplateFSM
#endif

/****************************************************************************/
// This are the name of the Event checking funcion header file. 
#define EVENT_CHECK_HEADER "EventCheckers.h"

/****************************************************************************/
// This is the list of event checking functions 
#define EVENT_CHECK_LIST Check4Keystroke 

/****************************************************************************/
// These are the definitions for the post functions to be executed when the
// corresponding timer expires. All 16 must be defined. If you are not using
// a timer, then you should use TIMER_UNUSED
// Unlike services, any combination of timers may be used and there is no
// priority in servicing them
#define TIMER_UNUSED ((pPostFunc)0)
#define TIMER0_RESP_FUNC PostSPIService
#define TIMER1_RESP_FUNC PostSPIService
#define TIMER2_RESP_FUNC PostSPIService
#define TIMER3_RESP_FUNC PostMasterSM
#define TIMER4_RESP_FUNC PostMasterSM
#define TIMER5_RESP_FUNC PostMasterSM
#define TIMER6_RESP_FUNC PostCOWSupplementService
#define TIMER7_RESP_FUNC PostDCMotorService
#define TIMER8_RESP_FUNC PostLEDService
#define TIMER9_RESP_FUNC PostServoGateService
#define TIMER10_RESP_FUNC PostMasterSM
#define TIMER11_RESP_FUNC PostMasterSM
#define TIMER12_RESP_FUNC PostUltrasonicTest
#define TIMER13_RESP_FUNC PostMasterSM
#define TIMER14_RESP_FUNC PostMasterSM
#define TIMER15_RESP_FUNC PostUltrasonicTest

/****************************************************************************/
// Give the timer numbers symbolc names to make it easier to move them
// to different timers if the need arises. Keep these definitions close to the
// definitions for the response functions to make it easier to check that
// the timer number matches where the timer event will be routed
// These symbolic names should be changed to be relevant to your application 

#define REPORT_QUERY_TIMER 0
#define REPORT_RESEND_TIMER 1
#define TRANSFER_INTERVAL_TIMER 2
#define RampUpTimer 3  // HOW LONG ?
#define BallTravelTimer 4  // 2 s
#define ShootingTimer 5  // 20 s
#define COW_INTERVAL_TIMER 6   // 3 s?
#define DC_STALL_TIMER 7 // 50ms
#define LED_BLINK_TIMER 8
#define SERVOGATE_TIMER 9
#define ArmsExpansionTimer 10 // 2 s
#define MagDelayTimer 11
#define ULTRASONICTRIG_TIMER 12
#define ByteTransferIntervalTimer 13  // 10 ms
#define ReportGap 14   // 300 ms
#define ULTRASONICSHUTOFF_TIMER 15
//#define GameTimer 15   // 2 min 18s (can it be so long?)

#endif /* CONFIGURE_H */
