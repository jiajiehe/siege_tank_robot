#define GamePeriod 60000
#define SB1 1
#define SB2 2
#define SB3 3
#define REPORT 4
#define RED 1
#define GREEN 0


#ifndef MasterSM_H
#define MasterSM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_Events.h"
#include "BITDEFS.H"

// typedefs for the states
// State definitions for use with the query function
typedef enum { WaitToStartGame, Driving, Shooting} MasterState_t ;

bool InitMasterSM(uint8_t Priority);
bool PostMasterSM(ES_Event ThisEvent);
ES_Event RunMasterSM(ES_Event ThisEvent);
void StartMasterSM(ES_Event);
bool QueryColor(void);
bool Query_isBackFromDepot(void);
bool Query_isFreeShooting(void);

#endif
