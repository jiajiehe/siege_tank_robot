#ifndef MoveToShootingSpot_H
#define MoveToShootingSpot_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "ES_Configure.h"
#include "ES_Framework.h"

// typedefs for the states
// State definitions for use with the query function
typedef enum {S_MoveInY} MoveToSSState_t ;

ES_Event RunMoveToShootingSpot(ES_Event ThisEvent);
void StartMoveToShootingSpot(ES_Event);
MoveToSSState_t QueryMoveToShootingSpot ( void );

#endif

