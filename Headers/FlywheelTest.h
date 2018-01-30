#ifndef FlywheelTest_H
#define FlywheelTest_H

// Framework headers
#include "ES_Framework.h"
#include "ES_Configure.h"
#include "ES_Types.h"

// State definitions for use with the query function
typedef enum {IdleMode, TestMode} FlywheelTestState_t;

// Public Function Prototypes
bool InitFlywheelTest ( uint8_t Priority );
bool PostFlywheelTest( ES_Event ThisEvent );
ES_Event RunFlywheelTest( ES_Event ThisEvent );
void FlywheelInputCaptureISR(void);
void PIControlISR(void);
#endif /* FlywheelTest_H */
