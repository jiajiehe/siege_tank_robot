#ifndef DrivingSM_H
#define DrivingSM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "ES_Configure.h"
#include "ES_Framework.h"

// typedefs for the states
// State definitions for use with the query function
typedef enum { WaitToMove, MoveToDestination} DrivingSMState_t ;

ES_Event RunDrivingSM(ES_Event ThisEvent);
void StartDrivingSM(ES_Event);
uint8_t QueryStartingStage(void);
#endif
