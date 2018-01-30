#ifndef UltrasonicTest_H
#define UltrasonicTest_H

#include "ES_Configure.h"
#include "ES_Types.h"

// Public Function Prototypes
bool InitUltrasonicTest ( uint8_t Priority );
bool PostUltrasonicTest( ES_Event ThisEvent );
ES_Event RunUltrasonicTest( ES_Event ThisEvent );
void UltrasonicCaptureResponse(void);
void OneShotTriggerISR(void);

typedef enum {	Waiting2Detect, Detecting } UltrasonicState_t;

#endif /* UltrasonicTest_H */

