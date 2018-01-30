#ifndef HallEffectService_H
#define HallEffectService_H

#define LEFT 1
#define RIGHT 2

typedef enum {	Waiting4Capture, PeriodCapturing } HallEffectSensorState_t;

// Function declarations
bool InitHallEffectService(uint8_t Priority);
bool PostHallEffectService(ES_Event ThisEvent);
ES_Event RunHallEffectService(ES_Event ThisEvent);
void HallEffectCaptureISR(void);
uint8_t getFreqCode(void);

#endif

