#ifndef DS18B20_H
#define DS18B20_H

#include <xc.h>
/* _XTAL_FREQ ya esta definido en config.h, no se redefine aqui */
#define OW_PIN  PORTDbits.RD6
#define OW_DIR  TRISDbits.TRISD6
#define OW_LOW  LATDbits.LATD6

unsigned char DS18B20_Reset(void);
void          DS18B20_WriteByte(unsigned char byte);
unsigned char DS18B20_ReadByte(void);
unsigned char DS18B20_StartConversion(void);
void          DS18B20_WaitConversion(void);   /* espera bloqueante ~750 ms max */
float         DS18B20_ReadTemp(void);

#endif /* DS18B20_H */