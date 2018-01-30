#ifndef DCMotorService_H
#define DCMotorService_H

// Framework headers
#include "ES_Framework.h"
#include "ES_Configure.h"
#include "ES_Types.h"

// State definitions for use with the query function
typedef enum { Running, Idle } DCMotorServiceState_t;

// Public Function Prototypes
bool InitDCMotorService ( uint8_t Priority );
bool PostDCMotorService( ES_Event ThisEvent );
ES_Event RunDCMotorService( ES_Event ThisEvent );

// For interrupts
void ControlISR(void);
void LMotorInputCapture(void);
void RMotorInputCapture(void);

// Following functions are to be called by MasterSM
void StartDrive(uint8_t Speed, uint8_t Direction);
void InitialDrive(uint8_t Speed, uint8_t TargetDirection);
void Drive(uint8_t Speed, uint8_t Direction);
void Stop(void);

#endif /* DCMotorService_H */
