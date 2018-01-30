

#include "DecipherFunctions.h"

bool DecipherGameStatus(uint8_t StatusByte3)
{
	bool GameStatus;
	uint8_t GSBit;
	GSBit = StatusByte3 & BIT7HI;
	if (GSBit == BIT7HI)
	{
		GameStatus = true;
	}
	else
	{
		GameStatus = false;
	}
  return(GameStatus);
}

bool DecipherDestinationType(uint8_t StatusByte1, bool CurrentColor)
{
	bool DestinationType; //Check whether the destination is a stage or a bucket
	// 0 -> check-in (stage), 1 -> shooting (bucket)
	if (CurrentColor) // CurrentColor = true -> red
	{
		DestinationType &= BIT3HI; // For red robot read bit 3 (CSR)
	}
	else
	{
		DestinationType &= BIT7HI; // For red robot read bit 7 (CSG)
	}
	return (DestinationType);
}

uint8_t DecipherDestination(uint8_t StatusByte1, bool CurrentColor)
{
	uint8_t Location;
	if (CurrentColor) // CurrentColor = true -> red
	{
		Location &= (BIT0HI|BIT1HI|BIT2HI); // For red robot read 4 LSBs, bit 3 is for checkin/shooting status
	}
	else
	{
		Location = StatusByte1 >> 4; // For green robot read 4 MSBs (and shifted right by 4 bit)
		Location &= (BIT0HI|BIT1HI|BIT2HI);
	}

	// 0x0 -> 0 (none active), 0x1 -> stage 1, 0x2 -> stage 2, 0x3 -> stage3
	if (Location > 3) // if ActiveStage = 0000 01xx
	{
		Location = SA_1;
	}
  return(Location);
}

bool DecipherReportStatus(uint8_t ReportStatusByte)
{
	// First decipher Acknowledge status
	bool AckStatus;
	if ( ( ReportStatusByte & (BIT7HI|BIT6HI) ) == 0) // if bit 7 and bit 6 of Reprot Status Byte are 00
	{
		// LOC acknowledges the report (ACK)
		AckStatus = true;
	}
	else
	{
		AckStatus = false;
	}
  return(AckStatus);
}

uint8_t DecipherLocationInReport(uint8_t ReportStatusByte)
{
	// Now decipher location code in the 3 LSBs
	// 0x001 -> red stage1, 0x010 -> red stage2, 0x110 -> red stage3, 0x100 -> green stage1, 0x101 -> green stage2, 0x110 -> green stage3
	uint8_t CurrentLocation;
	CurrentLocation = ( ReportStatusByte & (BIT2HI|BIT1HI|BIT0HI) );
	// Because color is already known, the returned CurrentLocation will only have values 1, 2, or 3
	// So only need to remap the CurrentLocation to 1,2 or 3 for the green side
	if (CurrentLocation >3)
	{
		CurrentLocation = CurrentLocation - 3;
	}
  return(CurrentLocation);
}

uint8_t DecipherScore(uint8_t StatusByte)
{
	// If green, status byte 2; if red, status byte 3
	// Now decipher the score in the 6 LSBs
	uint8_t CurrentScore;
	CurrentScore = ( StatusByte & (BIT7LO & BIT6LO) );
  return(CurrentScore);
}
