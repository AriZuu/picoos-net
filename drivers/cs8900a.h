#ifndef __CS8900A_H__
#define __CS8900A_H__

#include "net/uip_arch.h"

void cs8900aInit(void);
void cs8900aSend(void);
uint16_t cs8900aPoll(void);

#endif /* __CS8900A_H__ */
