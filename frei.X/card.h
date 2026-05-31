#ifndef CAR_H
#define CAR_H

#include <xc.h>

extern long CAR_ultimo_delta;

unsigned char CAR_HayDedo(unsigned long ir);
unsigned char CAR_ProcesarMuestra(unsigned long red, unsigned long ir);
unsigned char CAR_GetBPM(void);

#endif