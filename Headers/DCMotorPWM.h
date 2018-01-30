#ifndef DCMotorPWM_H
#define DCMotorPWM_H

// Framework headers
#include "ES_Framework.h"
#include "ES_Configure.h"
#include "ES_Types.h"

// Public Function Prototypes
void InitDCMotorPWMModule(void);
void SetDirection(uint8_t Motor, uint8_t Direction);
void SetDuty(uint8_t Motor, uint8_t Duty);
#endif /* DCMotorPWM_H */
