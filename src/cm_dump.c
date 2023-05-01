#include "connection_machine.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* This will dump the entire state of the machine into a manky big binary file. As connection machines
 * are typically pretty sparse, compression will be required. Each cycle stored raw is some 34MB. Perhaps
 * it is best to pipe the frames into some kind of online compression?
 *
 * The data will be stored as 16 pairs of flag, mem, flag, mem, followed by the state of the router, for
 * all routers.
 */

void cm_dump(cm *machine, uint32_t count, uint64_t ins, const char *fileName)
{
  char frameName[14];
  sprintf(frameName, "%u", count);
  FILE *frame = fopen(frameName, "w");
  uint32_t i, j;
  Message *dummy = (Message *)calloc(1, sizeof(Message));
  dummy->address = 0xFF;

  for (i = 0; i < (1 << DIMENSIONS); i++)
  {
    for (j = 0; j < (1 << PROCESSORS); j++)
    {
      fwrite(machine->chips[i]->cells[j], sizeof(Cell), 1, frame);
    }

    for (j = 0; j < 7; j++)
    {
      if (machine->chips[i]->router->buffer[j] == NULL)
      {
        fwrite(dummy, sizeof(Message), 1, frame);
      }
      else
      {
        fwrite(machine->chips[i]->router->buffer[j], sizeof(Message), 1, frame);
      }
    }
    fwrite(machine->chips[i]->router->listening, sizeof(uint32_t), 4, frame);
    for (j = 0; j < 4; j++)
    {
      if (machine->chips[i]->router->partials[j] == NULL)
      {
        fwrite(dummy, sizeof(Message), 1, frame);
      }
      else
      {
        fwrite(machine->chips[i]->router->partials[j], sizeof(Message), 1, frame);
      }
    }
  }
  fwrite(&ins, sizeof(uint64_t), 1, frame);

  fclose(frame);
  char command[128+strlen(fileName)];
  sprintf(command, "zip -9u %s %s", fileName, frameName);
  system(command);
  remove(frameName);
}
