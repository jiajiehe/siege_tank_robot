#ifndef ServoGateService_H
#define ServoGateService_H

#include "MasterSM.h"


typedef enum {	Waiting2Open, Opening } ServoShootingState_t;

// Function declarations
bool InitServoGateService(uint8_t Priority);
bool PostServoGateService(ES_Event ThisEvent);
ES_Event RunServoGateService(ES_Event ThisEvent);

#endif
