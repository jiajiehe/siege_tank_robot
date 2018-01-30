/****************************************************************************
 Template header file for Hierarchical Sate Machines AKA StateCharts

 ****************************************************************************/

#ifndef LEDService_H
#define LEDService_H

typedef enum {	Waiting4Designation, Red, Green } LEDState_t;

// Public Function Prototypes

bool InitLEDService ( uint8_t Priority );
bool PostLEDService( ES_Event ThisEvent );
ES_Event RunLEDService( ES_Event ThisEvent );

#endif /*LEDService_H */

