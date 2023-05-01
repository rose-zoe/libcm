#include <stdint.h>
#include <stdlib.h>
#include "chip.h"
#include <stdio.h>

/* The chip will act as a wrapper for instructions from the machine/host. It will wrap up in 1 call the
 * instruction execution, and petit cycle management from a global petit clock in the machine.
 */

void chip_exe(Chip *c, uint16_t addrA, uint16_t addrB, uint8_t flagR, uint8_t flagW, uint8_t flagC,
              uint8_t sense, uint8_t memTruth, uint8_t flagTruth, uint8_t newsDir, uint32_t petitClock,
              uint8_t shouldOr, uint8_t slowMode)
{
  /* First, deliver the instructions to the cells. Save their results (the flag outputs) into an array */
  uint8_t results[1 << PROCESSORS];
  uint32_t i;

  for (i = 0; i < 1 << PROCESSORS; i++)
  {
    results[i] = cell_exe(c->cells[i], addrA, addrB, flagR, flagW, flagC, sense, memTruth, flagTruth);
  }

  /* We can then write the special flags. The daisy chain is easiest - just write each index to i+1. */
  for (i = 0; i < (1 << PROCESSORS) - 1; i++)
  {
    if (results[i]) c->cells[i+1]->flags |= 1 << 3;
    else c->cells[i+1]->flags &= ~(1 << 3);
  }

  /* The NEWS may take a bit more thinking. For ease, we assume PROCESSORS is even - that is, the cells
   * on the chip can actually be arranged into a square. Then, we should be able to iterate over in both
   * directions assigning as required. To reduce jumps, condition on newsDir immediately and have some
   * redundant code. The chips are aranged left to right, top to bottom.
   */
  uint32_t j;
  uint32_t sqw = 1 << (PROCESSORS >> 1); /* The width of the square */
  if (newsDir == 0) /* North */
  {
    /* In this case, the bits move north. Each processor should set its bit to the processor below it, or
     * +sqw in the chain
     */
    for (i = 0; i < (1 << PROCESSORS) - sqw; i++)
    {
      if (results[i+sqw]) c->cells[i]->flags |= 1 << 8;
      else c->cells[i]->flags &= ~(1 << 8);
    }
  }
  else if (newsDir == 3) /* South */
  {
    /* Similar to as above, but condition on the processor above instead */
    for (i = sqw; i < (1 << PROCESSORS); i++)
    {
      if (results[i-sqw]) c->cells[i]->flags |= 1 << 8;
      else c->cells[i]->flags &= ~(1 << 8);
    }
  }
  else if (newsDir == 1) /* East */
  {
    /* In this case, every processor excent multiples of sqw just read the -1 processor. */
    for (i = 0; i < (1 << PROCESSORS); i++)
    {
      if (!(i % sqw))
      {
        if (results[i-1]) c->cells[i]->flags |= 1 << 8;
        else c->cells[i]->flags &= ~(1 << 8);
      }
    }
  }
  else if (newsDir == 2) /* Finally, west */
  {
    /* In this case, every processor except the last one take on the value of the +1 processor*/
    for (i = 0; i < (1 << PROCESSORS); i++)
    {
      if (i % sqw != sqw-1)
      {
        if (results[i+1]) c->cells[i]->flags |= 1 << 8;
        else c->cells[i]->flags &= ~(1 << 8);
      }
    }
  }

  /* That's cell execution done. Now we need to manage the router business. The first ADDRLEN +
   * MESSAGE_LENGTH << 3 + 3 cycles are always message injection from processors, so they can be handled
   * first.
   */
  if (petitClock < ADDRLEN + (MESSAGE_LENGTH << 3) + 3) router_inject(c->router, petitClock);
  /* Otherwise it'll be a dimension cycle or a delivery. Either way, we deal with petitClock - the
   * injection counter */
  else
  {
    //45
    uint32_t newClock = petitClock - (ADDRLEN + (MESSAGE_LENGTH << 3) + 3);
    /* Work out whether this is a delivery or a dimension cycle. Depends on whether slowMode is on or not
     */
    if ( (slowMode && newClock >= DIMENSIONS * (ADDRLEN + (MESSAGE_LENGTH << 3) + 2))
      || (!slowMode && newClock >= DIMENSIONS) )
    {
      /* Reset newClock to count for delivery cycle */
      if (slowMode) newClock -= (DIMENSIONS * (ADDRLEN + (MESSAGE_LENGTH << 3) + 2));
      else newClock -= DIMENSIONS;
      
      //if (petitClock == 96) printf("here? %u\n", newClock);
      router_deliver(c->router, newClock, shouldOr);
      //if (petitClock == 96) printf("no...\n");
    }
    else /* In a dimension cycle */
    {
      /* If in slowmode, we forward and receive the message on the first cycle, but wait around for
       * the correct number of clock pulses
       */
      if (slowMode)
      {
        if (newClock % (ADDRLEN + (MESSAGE_LENGTH << 3) + 2) == 0)
        {
          router_forward(c->router, newClock / (ADDRLEN + (MESSAGE_LENGTH << 3) + 2));
          /* Can't receive yet! Have to wait until all routers have forwarded for that */
        }
      }
      else /* Not Slowmode */
      {
        router_forward(c->router, newClock);
      }
    }
  }
  /* That should be the petit cycle done EXCEPT for receiving */
}

void chip_recv(Chip *c, uint32_t petitClock, uint8_t slowMode)
{
  /* If we're in injection or delivery cycles, do nothing */
  if (petitClock < ADDRLEN + (MESSAGE_LENGTH << 3) + 3) return;

  uint32_t newClock = petitClock - (ADDRLEN + (MESSAGE_LENGTH << 3) + 3);

  if ( (slowMode && newClock >= DIMENSIONS * (ADDRLEN + (MESSAGE_LENGTH << 3) + 2))
      || (!slowMode && newClock >= DIMENSIONS) ) return;

  /* Now we just need to receive the right dimension if either a) we're in fast mode or b) slow mode
   * but the beginning of the dimension cycle
   */
  if (!(slowMode)) router_receive(c->router, newClock);
  else if (newClock % (ADDRLEN + (MESSAGE_LENGTH << 3) + 2) == 0)
  {
    router_receive(c->router, newClock / (ADDRLEN + (MESSAGE_LENGTH << 3) + 2));
  }
  /* Else waste the cycle */
}

/* It seems sensible to include in here functions for creating chips, and attaching their routers
 * together
 */

Chip *chip_build()
{
  Chip *c = (Chip *)calloc(1, sizeof(Chip));
  c->router = (Router *)calloc(1, sizeof(Router));

  uint32_t i;
  for(i = 0; i < (1 << PROCESSORS); i++)
  {
    c->cells[i] = (Cell *)calloc(1, sizeof(Cell));
    c->router->flags[i] = &(c->cells[i]->flags);
  }

  return c;
}

void chip_del(Chip *c)
{
  free(c->router);
  for (uint32_t i = 0; i < (1 << PROCESSORS); i++) free(c->cells[i]);
  free(c);
}

void chip_connect(Chip *self, Chip *other, uint32_t dim)
{
  self->router->outports[dim] = &(other->router->inports[dim]);
}
