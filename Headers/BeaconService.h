#ifndef BeaconService_H
#define BeaconService_H

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

#include "MasterSM.h"  // need the #define there


typedef enum {	B_Waiting4Capture, B_PeriodCapturing } BeaconState_t;

// Function declarations
bool InitBeaconService(uint8_t Priority);
bool PostBeaconService(ES_Event ThisEvent);
ES_Event RunBeaconService(ES_Event ThisEvent);
void LeftIRCaptureISR(void);
void RightIRCaptureISR(void);

#endif
