#ifndef SPIHelper_H
#define SPIHelper_H

// Public Function Prototypes
void SPIInit(void);
void SPI_SendCMD(uint8_t command);
uint8_t SPI_ReadRES(void);
void EOTISR(void);

#endif /*SPIHelper_H */

