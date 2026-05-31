#include <xc.h>
#include "config.h"
#include "i2c.h"

#define SCL_DIR  TRISBbits.TRISB0
#define SDA_DIR  TRISBbits.TRISB1
#define SCL      LATBbits.LATB0
#define SDA      LATBbits.LATB1
#define SDA_IN   PORTBbits.RB1

void I2C_Init(void) {
    SCL_DIR = 0;
    SDA_DIR = 0;
    SCL = 1;
    SDA = 1;
    INTCON2bits.RBPU = 0;
}

void I2C_Start(void) {
    SDA_DIR = 0;
    SDA = 1; __delay_us(5);
    SCL = 1; __delay_us(5);
    SDA = 0; __delay_us(5);
    SCL = 0; __delay_us(5);
}

void I2C_Stop(void) {
    SDA_DIR = 0;
    SDA = 0; __delay_us(5);
    SCL = 1; __delay_us(5);
    SDA = 1; __delay_us(5);
}

void I2C_RepeatedStart(void) {
    I2C_Stop();
    __delay_us(5);
    I2C_Start();
}

void I2C_Write(unsigned char data) {
    unsigned char i;
    for (i = 0; i < 8; i++) {
        SDA = (data & 0x80) ? 1 : 0;
        data <<= 1;
        __delay_us(5);
        SCL = 1; __delay_us(5);
        SCL = 0; __delay_us(5);
    }
    SDA_DIR = 1; __delay_us(5);
    SCL = 1;     __delay_us(5);
    SCL = 0;
    SDA_DIR = 0; __delay_us(5);
}

unsigned char I2C_Read(char ack) {
    unsigned char data = 0;
    unsigned char i;
    SDA_DIR = 1;
    for (i = 0; i < 8; i++) {
        data <<= 1;
        __delay_us(5);
        SCL = 1; __delay_us(5);
        if (SDA_IN) data |= 0x01;
        SCL = 0; __delay_us(5);
    }
    SDA_DIR = 0;
    SDA = (ack) ? 0 : 1;
    __delay_us(5);
    SCL = 1; __delay_us(5);
    SCL = 0;
    SDA = 1; __delay_us(5);
    return data;
}