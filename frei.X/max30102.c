#include <xc.h>
#include "config.h"
#include "i2c.h"
#include "max30102.h"

unsigned char MAX30102_WriteRegister(unsigned char reg, unsigned char data) {
    I2C_Start();
    I2C_Write(MAX30102_ADDR_WRITE);
    I2C_Write(reg);
    I2C_Write(data);
    I2C_Stop();
    return 1;
}

unsigned char MAX30102_ReadRegister(unsigned char reg, unsigned char *data) {
    I2C_Start();
    I2C_Write(MAX30102_ADDR_WRITE);
    I2C_Write(reg);
    I2C_RepeatedStart();
    I2C_Write(MAX30102_ADDR_READ);
    *data = I2C_Read(0);
    I2C_Stop();
    return 1;
}

unsigned char MAX30102_Check(void) {
    unsigned char part_id;
    if (!MAX30102_ReadRegister(MAX30102_PART_ID, &part_id)) return 0;
    return (part_id == 0x15);
}

unsigned char MAX30102_Init(void) {
    unsigned char status;
    if (!MAX30102_Check()) return 0;
    MAX30102_WriteRegister(MAX30102_MODE_CONFIG, 0x40);
    __delay_ms(20);
    MAX30102_WriteRegister(MAX30102_FIFO_WR_PTR, 0x00);
    MAX30102_WriteRegister(MAX30102_OVF_COUNTER, 0x00);
    MAX30102_WriteRegister(MAX30102_FIFO_RD_PTR, 0x00);
    MAX30102_WriteRegister(MAX30102_FIFO_CONFIG, 0x50);
    MAX30102_WriteRegister(MAX30102_SPO2_CONFIG, 0x27);
    MAX30102_WriteRegister(MAX30102_LED1_PA,     0x1F);
    MAX30102_WriteRegister(MAX30102_LED2_PA,     0x1F);
    MAX30102_WriteRegister(MAX30102_MODE_CONFIG, 0x03);
    MAX30102_ReadRegister(MAX30102_INT_STATUS_1, &status);
    MAX30102_ReadRegister(MAX30102_INT_STATUS_2, &status);
    return 1;
}

unsigned char MAX30102_SamplesAvailable(void) {
    unsigned char write_ptr, read_ptr;
    MAX30102_ReadRegister(MAX30102_FIFO_WR_PTR, &write_ptr);
    MAX30102_ReadRegister(MAX30102_FIFO_RD_PTR, &read_ptr);
    return (write_ptr - read_ptr) & 0x1F;
}

unsigned char MAX30102_ReadSample(unsigned long *red, unsigned long *ir) {
    unsigned char data[6];
    if (MAX30102_SamplesAvailable() == 0) return 0;
    I2C_Start();
    I2C_Write(MAX30102_ADDR_WRITE);
    I2C_Write(MAX30102_FIFO_DATA);
    I2C_RepeatedStart();
    I2C_Write(MAX30102_ADDR_READ);
    data[0] = I2C_Read(1);
    data[1] = I2C_Read(1);
    data[2] = I2C_Read(1);
    data[3] = I2C_Read(1);
    data[4] = I2C_Read(1);
    data[5] = I2C_Read(0);
    I2C_Stop();
    *red = (((unsigned long)data[0] & 0x03) << 16) |
           ((unsigned long)data[1] << 8) | data[2];
    *ir  = (((unsigned long)data[3] & 0x03) << 16) |
           ((unsigned long)data[4] << 8) | data[5];
    return 1;
}
