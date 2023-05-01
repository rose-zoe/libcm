#ifndef CM_CM_H_
#define CM_CM_H_

#include "chip.h"

typedef struct
{
  Chip *chips[1 << DIMENSIONS];
  uint32_t petitCounter;
  uint8_t shouldOr;
  uint8_t slowMode;
  uint8_t globalPin;
  uint8_t dump;
} cm;

cm *cm_build();

void cm_del(cm *machine);

void cm_exe(cm *machine, uint16_t addrA, uint16_t addrB, uint8_t flagR, uint8_t flagW, uint8_t flagC,
            uint8_t sense, uint8_t memTruth, uint8_t flagTruth, uint8_t newsDir);

uint8_t shouldOr(cm *machine);

uint8_t shouldntOr(cm *machine);

uint8_t slowMode(cm *machine);

uint8_t fastMode(cm *machine);

void petit_sync(cm *machine);

uint8_t shouldDump(cm *machine);

uint8_t shouldntDump(cm *machine);

int network_empty(cm *machine);

void cycles();

/* Also define some useful functions for instructions */

#define AND 0b00000001
#define OR 0b01111111
#define XOR 0b01101001
#define IDM 0b00001111
#define IDF 0b01010101
#define CPM 0b00110011 //Copies b into a if used memTruth, b into flag if flagTruth
#define MAJ 0b00010111
#define SETO 0b11111111
#define SETZ 0b00000000

#endif
