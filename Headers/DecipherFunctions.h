#ifndef Decipher_H
#define Decipher_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "BITDEFS.H"

// Identify positions
#define NONE 0
#define SA_1 1
#define SA_2 2
#define SA_3 3
#define SHOOTING_2 4
#define BACK_WALL 5		
#define DEPOT 0

bool DecipherGameStatus(uint8_t StatusByte3);
bool DecipherDestinationType(uint8_t StatusByte1, bool CurrentColor);
uint8_t DecipherDestination(uint8_t StatusByte1, bool CurrentColor);
bool DecipherReportStatus(uint8_t ReportStatusByte);
uint8_t DecipherLocationInReport(uint8_t ReportStatusByte);
uint8_t DecipherScore(uint8_t StatusByte);

#endif
