#include "connection_machine.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "cm_dump.c"

/* Firstly, we need to build a connection machine out of chips, and connect all the wires together in a
 * hypercube
 */

uint32_t count;


cm *cm_build()
{
  cm *machine = (cm *)calloc(1, sizeof(cm));
  uint32_t i;
  for (i = 0; i < (1 << DIMENSIONS); i++)
  {
    machine->chips[i] = chip_build();
  }

  for (i = 0; i < (1 << DIMENSIONS); i++)
  {
    machine->chips[i]->router->referer = machine->chips[ (i+1) % (1 << DIMENSIONS) ]->router;
    machine->chips[i]->router->id = i;
  }
  /* All the chips are build. Just need to connect each one with adjacent ones. This can be done by
   * iterating over all chips, and running connect with every index that has exactly 1 different bit
   */
  uint32_t dim;
  for (i = 0; i < (1 << DIMENSIONS); i++)
  {
    for (dim = 0; dim < DIMENSIONS; dim++)
    {
      chip_connect(machine->chips[i], machine->chips[i ^ (1 << dim)], DIMENSIONS - 1 - dim);
    }
  }
  count = 0;

  return machine;
}

/* We can delete a machine by deleting all of its chips then freeing it */
void cm_del(cm *machine)
{
  for (uint32_t i = 0; i < (1 << DIMENSIONS); i++) chip_del(machine->chips[i]);
  free(machine);
}

/* We can then easily implement a wrapper for chips to execute based on instruction calls to the machine
 */

void cm_exe(cm *machine, uint16_t addrA, uint16_t addrB, uint8_t flagR, uint8_t flagW, uint8_t flagC,
            uint8_t sense, uint8_t memTruth, uint8_t flagTruth, uint8_t newsDir)
{
  uint32_t i;
  for (i = 0; i < (1 << DIMENSIONS); i++)
  {
    //if (count == 193) printf("%u %u\n", i, machine->petitCounter);
    chip_exe(machine->chips[i], addrA, addrB, flagR, flagW, flagC, sense, memTruth, flagTruth, newsDir,
             machine->petitCounter, machine->shouldOr, machine->slowMode);
  }
  for (i = 0; i < (1 << DIMENSIONS); i++)
  {
    chip_recv(machine->chips[i], machine->petitCounter, machine->slowMode);
  }

  /* The global pin should be obtained from the logical or of all the global flags (flag 1) of the
   * cells
   */
  machine->globalPin = 0;
  for (i = 0; i < (1 << DIMENSIONS); i++)
  {
    uint32_t j;
    for (j = 0; j < (1 << PROCESSORS); j++)
    {
      machine->globalPin |= ( ( machine->chips[i]->cells[j]->flags >> 14) & 1);
      /* The pin should also be set to low, we only assert globals for 1 cycle */
      machine->chips[i]->cells[j]->flags &= ~(1 << 14);
    }
  }
  if (machine->dump)
  {
    uint64_t ins = 0;
    ins |= addrA;
    ins = ins << 12; ins |= addrB;
    ins = ins << 4; ins |= flagR;
    ins = ins << 4; ins |= flagW;
    ins = ins << 4; ins |= flagC;
    ins = ins << 1; ins |= sense;
    ins = ins << 8; ins |= memTruth;
    ins = ins << 8; ins |= flagTruth;
    ins = ins << 2; ins |= newsDir;
    cm_dump(machine, count, ins, "dump.dat");
  }
  count++;
  machine->petitCounter++;
  if (machine->petitCounter >= (ADDRLEN + (MESSAGE_LENGTH << 3) + 3 +
                               (DIMENSIONS * (machine->slowMode ? ADDRLEN + (MESSAGE_LENGTH << 3) + 2
                                                                : 1)) +
                               (MESSAGE_LENGTH << 3) + 2)) machine->petitCounter = 0;}

/* Setting slow mode and or mode should only be allowable at the beginning of a cycle */
uint8_t shouldOr(cm *machine)
{
  if (machine->petitCounter) return -1;
  else
  {
    machine->shouldOr = 1;
    return 0;
  }
}

uint8_t shouldntOr(cm *machine)
{
  if (machine->petitCounter) return -1;
  else
  {
    machine->shouldOr = 0;
    return 0;
  }
}

uint8_t slowMode(cm *machine)
{
  if (machine->petitCounter) return -1;
  else
  {
    machine->slowMode = 1;
    return 0;
  }
}

uint8_t fastMode(cm *machine)
{
  if (machine->petitCounter) return -1;
  else
  {
    machine->slowMode = 0;
    return 0;
  }
}

uint8_t shouldDump(cm *machine)
{
  if (count) return -1;
  else
  {
    machine->dump = 1;
    return 0;
  }
}

uint8_t shouldntDump(cm *machine)
{
  if (count) return -1;
  else
  {
    machine->dump = 0;
    return 0;
  }
}

/* It will also be useful to have a way to synchronise the machine to petit cycle 0 by basically doing
 * noops until it gets there. NOTE that this does not flush routers!
 */

void petit_sync(cm *machine)
{
  while (machine->petitCounter != 0)
  {
    cm_exe(machine, 0, 0, 0, 0, 0, 0, IDM, IDF, 0);
  }
}

int network_empty(cm *machine)
{
  uint32_t i;
  for (i = 0; i < (1 << DIMENSIONS); i++)
  {
    if (router_empty(machine->chips[i]->router)) return 1;
  }
  return 0;
}

void cycles()
{
    printf("cycle count: %u\n", count);
}
