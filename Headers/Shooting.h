#ifndef SHOOTING_H
#define SHOOTING_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "ES_Configure.h"
#include "ES_Framework.h"

// typedefs for the states
// State definitions for use with the query function
typedef enum { FlywheelRamping, Scoring, GetCOWs} ShootingState_t ;

ES_Event RunShooting(ES_Event ThisEvent);
void StartShooting(ES_Event);

#endif
