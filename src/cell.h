#ifndef CM_CELL_H_
#define CM_CELL_H_

#include <stdint.h>

/* Structure of the cell itself */
typedef struct
{
   uint16_t flags; /* 16 flags */
   uint8_t memory[512]; /* 512 * 8 bits gives 4kbit memory */
} Cell;

/* Function headers */
uint8_t cell_exe(Cell *c, uint16_t addrA, uint16_t addrB, uint8_t flagR, uint8_t flagW,
              uint8_t flagC, uint16_t sense, uint8_t memTruth, uint8_t flagTruth);

#endif
