#ifndef MoveToDestination_H
#define MoveToDestination_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "ES_Configure.h"
#include "ES_Framework.h"

// typedefs for the states
// State definitions for use with the query function
typedef enum { MoveToStage, CheckIn, Handshake, GoToShootingSpot} MoveToDestinationState_t ;

ES_Event RunMoveToDestination(ES_Event ThisEvent);
void StartMoveToDestination(ES_Event);
MoveToDestinationState_t QueryCurrentMTDstate ( void );
uint8_t QueryCurrentLocation(void);
#endif
