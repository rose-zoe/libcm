#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "cell.h"

/* This defines the structure of cells, as well as giving operations on pointers
 * to them to do useful things.
 *
 * Important arch calls for the simulator: memory will be similated internally to cells, however the
 * execution function will return the flag value, with the chip module using glue logic to deal with the
 * news pins. In the verilog, it seems reasonable to implement this functionality in the router, but that
 * won't work here, as the chip module makes calls to cells rather than them happening concurrently.
 */

/* Function to instruct a cell to perform an instruction */
uint8_t cell_exe(Cell *c, uint16_t addrA, uint16_t addrB, uint8_t flagR, uint8_t flagW,
              uint8_t flagC, uint16_t sense, uint8_t memTruth, uint8_t flagTruth)
{
   /* Step 1: check if we actually need to run the instruction. To do this, we
      shift right by the flag number, and with 1, and then check equality to sense */
   // printf("%d\n", ((c->flags >> (15-flagC)) & 1));
   if (((c->flags >> (15-flagC)) & 1) != sense) return 0;

   /* Step 2: extract the 2 bits from memory. Shift right 3 to find the index, then
    * take the difference to find the actual word offset */
   uint8_t datA = ((c->memory)[addrA >> 3] >> (7-(addrA & 7))) & 1;
   uint8_t datB = ((c->memory)[addrB >> 3] >> (7-(addrB & 7))) & 1;
   // printf("%d\n", datB);
   uint8_t datF = (c->flags >> (15-flagR)) & 1;

   /* Now comes the actual trust table lookup. Truthtables are provided in the order
    * A B F binary counting, e.g. 011 is A low, B and F high. So, we can simply add
    * together A shifted twice, B shifted once, and F shifted 0 (all to the left) to
    * find the bit index in the table; shifting the table right by 7-this will give
    * the actual truth value
    */

   uint8_t index = (datA << 2) | (datB << 1) | datF;
   // printf("%d\n", index);
   uint8_t memV = (memTruth >> (7-index)) & 1;
   uint8_t flagV = (flagTruth >> (7-index)) & 1;

   /* Finally, write back. This is a bit of a pain as we need to set a bit in a larger byte!
    * The best way to do this seems to be to shift a 1 to the correct position; if writing 1,
    * simply or it in, if writing 0, not and then and it in.
    */

   /* Do this with the memory first */
   uint8_t memMask = 1 << (7 - (addrA & 7));
   uint8_t memByte = (c->memory)[addrA >> 3];
   if (memV) /* Writing a 1 */
   {
      memByte |= memMask;
   }
   else /* Writing a 0 */
   {
      memByte &= ~memMask;
   }
   (c->memory)[addrA >> 3] = memByte;

   /* Similar thing with the flags. However, we first must check it's a writeable flag! */
   if (!(flagW == 0 || flagW == 3 || flagW == 4 || flagW == 6 || flagW == 7))
      /* These flags are 0, daisychain, routerack, cube, and NEWS, which as ro */
   {
      uint16_t flagMask = 1 << (15 - flagW);
      if (flagV) /* Writing a 1 */
      {
         (c->flags) |= flagMask;
      }
      else
      {
         (c->flags) &= ~flagMask;
      }
   }
   // printf("%d %d\n", memByte, c->flags);

   /* I think this is done? Apart from daisy chain & NEWS outputting which haven't yet been
    * implemented.
    */
   return flagV;
}

/* This is basically all a cell can do - store some data and execute on that data given instructions
 * from above. Additional functions may be defined for easily writing to cells for initialisation
 * and reading results from them later on.
 */
