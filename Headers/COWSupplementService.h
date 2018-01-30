#ifndef COWSupplementService_H
#define COWSupplementService_H

typedef enum {	Waiting4Reload, Waiting4FullLoad } COWSupplementState_t;

// Function declarations
bool InitCOWSupplementService(uint8_t Priority);
bool PostCOWSupplementService(ES_Event ThisEvent);
ES_Event RunCOWSupplementService(ES_Event ThisEvent);
void PulseISR(void);

#endif
