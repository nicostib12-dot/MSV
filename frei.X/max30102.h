#ifndef MAX30102_H
#define MAX30102_H

#include <xc.h>

#define MAX30102_ADDR_WRITE   0xAE
#define MAX30102_ADDR_READ    0xAF

#define MAX30102_INT_STATUS_1  0x00
#define MAX30102_INT_STATUS_2  0x01
#define MAX30102_INT_ENABLE_1  0x02
#define MAX30102_INT_ENABLE_2  0x03
#define MAX30102_FIFO_WR_PTR   0x04
#define MAX30102_OVF_COUNTER   0x05
#define MAX30102_FIFO_RD_PTR   0x06
#define MAX30102_FIFO_DATA     0x07
#define MAX30102_FIFO_CONFIG   0x08
#define MAX30102_MODE_CONFIG   0x09
#define MAX30102_SPO2_CONFIG   0x0A
#define MAX30102_LED1_PA       0x0C
#define MAX30102_LED2_PA       0x0D
#define MAX30102_REV_ID        0xFE
#define MAX30102_PART_ID       0xFF

unsigned char MAX30102_ReadRegister(unsigned char reg, unsigned char *data);
unsigned char MAX30102_WriteRegister(unsigned char reg, unsigned char data);
unsigned char MAX30102_Check(void);
unsigned char MAX30102_Init(void);
unsigned char MAX30102_SamplesAvailable(void);
unsigned char MAX30102_ReadSample(unsigned long *red, unsigned long *ir);

#endif