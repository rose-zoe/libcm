#ifndef CM_CHIP_H_
#define CM_CHIP_H_

#include "router.h"
#include "cell.h"

typedef struct
{
  Router *router;
  Cell *cells[1 << PROCESSORS];
} Chip;

void chip_exe(Chip *c, uint16_t addrA, uint16_t addrB, uint8_t flagR, uint8_t flagW, uint8_t flagC,
              uint8_t sense, uint8_t memTruth, uint8_t flagTruth, uint8_t newsDir, uint32_t petitClock,
              uint8_t shouldOr, uint8_t slowMode);

void chip_recv(Chip *c, uint32_t petitClock, uint8_t slowMode);

Chip *chip_build();

void chip_del(Chip *c);

void chip_connect(Chip *self, Chip *other, uint32_t dim);

#endif
