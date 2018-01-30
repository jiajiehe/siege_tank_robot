#ifndef ServoPWM_H
#define ServoPWM_H

// Framework headers
#include "ES_Framework.h"
#include "ES_Configure.h"
#include "ES_Types.h"

// Public Function Prototypes
void InitServoPWM(void);
void SetServoDuty(uint8_t ServoMotor, uint8_t Duty);
void OpenServoGate(void);
void HalfOpenServoGate(void);
void CloseServoGate(void);
void ExpandRightRollerArm(void);
void ExpandLeftRollerArm(void);
void CloseRightRollerArm(void);
void CloseLeftRollerArm(void);
void ExpandSensorArm(void);
void CloseSensorArm(void);
#endif /* ServoPWM_H */
