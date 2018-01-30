/****************************************************************************
 SPIService header file for Hierarchical Sate Machines AKA StateCharts
 02/08/12 adjsutments for use with the Events and Services Framework Gen2
 3/17/09  Fixed prototpyes to use Event_t
 ****************************************************************************/

#ifndef SPIService_H
#define SPIService_H


// typedefs for the states
// State definitions for use with the query function
typedef enum { Waiting2Send, Waiting4ResponseReady, Waiting4EOT, Waiting4Timeout } SPIState_t ;


// Public Function Prototypes
bool InitSPIService ( uint8_t Priority );
bool PostSPIService( ES_Event ThisEvent );
ES_Event RunSPIService( ES_Event CurrentEvent );
void StartSPIService ( ES_Event CurrentEvent );
uint32_t getResponse(void);

#endif /*SPIService_H */

