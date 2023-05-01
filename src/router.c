/* A quick description of the routing stuff - routers communicate over wires, on each of which they
 * are connected to one other router. As an abstraction, a router will have just a single inport, a
 * pointer to a message. It will have an array of outports, with the entries having the type of
 * pointers to pointers to messages - each entry in the outport array is a pointer to the inport of
 * the dimensionally adjacent router. The router also has an array of pointers to messages, acting as
 * a 7 element FIFO. That way, to send a message, the router canjust reallocate pointers as required.
 *
 * The only tricky parts is communication between routers and cells. As a partial message is just a
 * bitstream, when a processor signals it wants to send a message (done by setting the router flag
 * high at the beginning of a petit cycle), if the router has space to accept it, it will calloc a
 * message and copy in the data using the routers "cycle pointer" counter. Outputting messages will
 * be done similarly - just iterate over the payload with the cycle counter.
 *
 * For the sake of ease, messages will be passed around as C structs (defined in the header file).
 * Obviously this would not occur in hardware, but as Hillis is very vague about the speed of the
 * routing algorithm I'll call this "reasonable". The address field will be 32 bits, sufficient for
 * some 4bn cells, well above what can be reasonably simulated. Messages will be stored as an array of
 * 32 bit integers, allowing the size to be changed at will. Parity is also stored.
 */

#include "router.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

void router_refer_deliver(Router *router, Message *m)
{
  uint32_t i = 0;
  while ( (i < BUFSIZE) && (router->buffer[i] != NULL) ) i++;
  if (i == BUFSIZE) router_refer_deliver(router->referer, m);
  else
  {
    m->address ^= (router->id << PROCESSORS);
    router->buffer[i] = m;
  }
}

void router_refer(Router *router, Message *m)
{
  if (router->partials[0] == m) return;

  /* Let's actually implement this! it will just send it to "another router" - this can be acheived
   * easily in hardware by just connecting up a new wire in a ham cycle. The effect is that the message
   * just appears "somewhere else" in the network
   */
  //printf("Referring!!\n");
  m->address ^= (router->id << PROCESSORS);
  router_refer_deliver(router->referer, m); return;
  /* For now, referal is not implemented, we just print an error and crash out */
  printf("Buffer exceeded\n");
  abort();
}

void router_forward(Router *router, uint32_t dimension)
{
  /* First port of call is to determine which message will be forwarded. It should be the message
   * earliest in the buffer with the appropriate dimension bit set high.
   */
  Message *toSend = NULL;
  uint32_t i;

  for (i = 0; i < BUFSIZE; i++)
  {
    /* From the address, we must erase PROCESSORS bits from the end and then find the correct dim bit
     * with a shift
     */
    if ((router->buffer)[i]) /* If not null */
    {
      if (((((router->buffer)[i])->address) >> (DIMENSIONS -1 - dimension + PROCESSORS)) & 1)
        /* Shift appropiate ammount and and with 1 for the bool. True if it needs to move along that
         * dimension
         */
      {
        toSend = (router->buffer)[i];
        break; /* I don't think this incremends i on the last iter? */
      }
    }
  }

  if (toSend) /* Don't do if null, no message was found! */
  {
    /* Then, we can flip the appropriate bit to 0 in the address */
    toSend->address &= ~(1 << (DIMENSIONS - 1 - dimension + PROCESSORS));

    /* And send it to the router connected along that wire */
    *(router->outports[dimension]) = toSend;

    /*Finally, remove from buffer by shifting everything after it */
    for (; i < BUFSIZE - 1; i++)
    {
      (router->buffer)[i] = (router->buffer)[i+1];
    }
    (router->buffer)[BUFSIZE - 1] = NULL;
  }
}

/* To receive a message, we simply read from the appropiate inport based on the dimension, add it to
 * the buffer IF THERE IS SPACE, then remove the pointer from the inports. If there is no space, we need
 * to somehow refer it. I'll write that function later after some further architectural decisions
 */

void router_receive(Router *router, uint32_t dim)
{
  Message *m = router->inports[dim];
  if (m == NULL) return; /* If no input message, there's nothing to do */

  //printf("recv ");
  uint8_t i = 0;
  while (i < BUFSIZE && router->buffer[i] != NULL) i++;
  //printf("%u\n", i);
  if (i == BUFSIZE) router_refer(router, m); /* Buffer is full, need to refer */
  else router->buffer[i] = m; /* Address switching has already been handled in the sending */

  router->inports[dim] = NULL; /* This function depends on inports being null if no message was sent! */
}

/* Message injection can managed by having each router have access to the flags of its associated
 * processors. Messages will be sent from processors as a 1 as a handshake, followed by the router
 * address and the processor number (big endian), a 1 for formatting, the message, and finally a parity
 * bit. The router will check the parity on a fully received message.
 *
 * The router can accept up to 4 messages from processors per petit cycle. As such, routers will contain
 * an array of 4 router numbers, and 4 partial messages (stored as messages with some undefined fields).
 * Processors wishing to send a message signal by sending a 1 on the first cycle of a petit cycle. The
 * router will pick up to 4 messages (less if it has less buffer space), with priority given to leftmost
 * processors. Each time the function is called it will read the appropriate bit and add it to the
 * message, and update the parity bit. On the final call of the injection cycle, it will check parity
 * and complete the handshake if the parity succeeded.
 */

void router_inject(Router *router, uint16_t bit) /*Messages up to ~64Ki, wayyy too big! */
{
  /* There is something special to do on bit 0 - this is the handshake 1. The router must decide which
   * messages to accept.
   */
  if (bit == 0)
  {
    /* First figure out how many messages we're willing to accept */
    uint16_t i=0, accNo;
    while (i < BUFSIZE && ((router->buffer)[i] != NULL)) i++; /* Breaks when a null is found or i = 7 */
    accNo = BUFSIZE-i; /* This is the number of free spots */
    if (accNo > 4) accNo = 4; /* Accept max of 4 messages per petit cycle */

    /* Now decide who we're accepting messages from by iterating over all the router flags and finding
     * the first accNo willing to send */
    i = 0;
    uint16_t j = 0;

    while (i < accNo && j < (1 << PROCESSORS)) /* j counts the processors, index in flags */
    {
      /* Follow the pointers and extract the flag we want, see if it's a 1 */
      uint16_t flags = *((router->flags)[j]); /* Value of all the flags in the proc */

      /* Router data is flag 5, requires right shifting 10 and anding with 1 */
      flags = (flags >> 10) & 1;

      if (flags) /* The processor is trying to send a message. The router has space, so accept */
      {
        (router->listening)[i] = j; /* Mark proc j as listened to */

        /* We need to create a partial message to be written into. Do this with calloc */
        (router->partials)[i] = (Message*)calloc(1, sizeof(Message));
        //printf("New message %u\n", router->partials[i]);
        /* As they're all 0s, this is correct parity! Also increment i */
        i++;
      }
      j++;
    }

    /* The router is accepting as many messages as it can, and partials are correctly set up. If less
     * than 4 messages are being sent, the associated entries in partials will be null pointers.
     */
  }

  /* The next case is the router is sending the address on bits indexed from 1 to the length of the
   * address. These need to be ored into the address field.
   */
  else if (bit < ADDRLEN + 1) /* +1 as this whole thing is offset by 1 at this point */
  {
    uint16_t i = 0;
    while (i < 4 && (router->partials)[i] != NULL)
    {
      /* First, extract the bit in question */
      uint16_t flags = *((router->flags)[(router->listening)[i]]);
      flags = (flags >> 10) & 1;

      /* This needs to be shifted to the correct bit, which is basically ADDRLEN - bit - 1, and ored in
       */
      ((router->partials)[i])->address |= ((uint32_t)flags) << (ADDRLEN - bit);

      i++;
    }
    /* And that's basically it for addresses. */
  }

  /* The next bit will always be a 1 in valid messages. Using a bit of fudging, we can set parity to 2
   * if it isn't, that way the parity will never be correct at the end and the message will be rejected
   */
  else if (bit == ADDRLEN + 1)
  {
    uint16_t i = 0;
    while (i < 4 && (router->partials)[i] != NULL)
    {
      uint16_t flags = *((router->flags)[(router->listening)[i]]);
      flags = (flags >> 10) & 1;

      if (!flags) /* if this bit is a 0 */
      {
        ((router->partials)[i])->parity = 2;
      }

      i++;
    }
    /* Otherwise parity is 0 by default and we can do nothing */
  }

  /* It's a very similar ordeal for the message itself - read a bit, shift it right, and or it in - but
   * there's a couple of extra things to deal with. Messages are stored in an array, and the parity bit
   * needs dealing with by xoring in the bit itself
   */
  else if (bit < ADDRLEN + (MESSAGE_LENGTH << 3) + 2)
    /* The 2 extra bits are the initial 1 and the
     * seperator 1
     */
  {
    uint16_t i = 0;
    while (i < 4 && (router->partials[i] != NULL))
    {
      uint16_t flags = *((router->flags)[(router->listening)[i]]);
      flags = (flags >> 10) & 1;

      /* The position in the byte is determined by the last 3 bits of the bit number - the ADDRLEN + 2
       * offset.
       */
      uint8_t byteOffset = (bit - ADDRLEN - 2) & 7;

      /* The actual byte is determined by this number shifted 3 right. The assignment can be done in one
       * fell swoop.
       */
      (((router->partials)[i])->message)[(bit - ADDRLEN - 2) >> 3] |= flags << (7 - byteOffset);

      /* Also, xor the bit value into the parity bit */
      ((router->partials)[i])->parity ^= flags;

      i++;

      /* This whole thing seems quite wasteful as it does a whole lotta nothing if the bit is 0, maybe
       * should rewrite to only do this if the flag is 1? Will that jump be faster than the (presumably
       * pretty fast) bit twiddling? Probably as we avoid writes.
       */
    }
  }

  /* Finally, on the last bit received, we need to check parity. Assuming that's all good we set the
   * handshake bit high. On the subsequent bit we just set the handshake bit low again.
   */
  else if (bit == ADDRLEN + (MESSAGE_LENGTH << 3) + 2)
  {
    uint16_t i = 0;
    while (i < 4 && (router->partials[i] != NULL))
    {
      uint16_t flags = *((router->flags)[(router->listening)[i]]);
      flags = (flags >> 10) & 1;

      if (flags == (uint16_t)((router->partials)[i])->parity)
      {
        /* Set the handshake bit to high */
        *((router->flags)[(router->listening)[i]]) |= 1 << 11;

        /* Then add the finished partial into the next open space in the buffer */
        uint8_t j = 0;
        while ((router->buffer)[j]) j++;
        (router->buffer)[j] = (router->partials)[i];
        (router->partials)[i] = NULL;
      }
      /* Else, something has gone wrong and we don't complete the handshake, act as if the message never
       * happened. BUT the broken message needs to be deleted with free!
       */
      else
      {
        free((router->partials)[i]);
      }

      (router->partials)[i] = NULL;
      i++;
    }
  }

  else if (bit == ADDRLEN + (MESSAGE_LENGTH << 3) + 3)
  {
    /* In this case, simply set every processor's handshake flag to low */
    for (uint32_t j = 0; j < (1 << PROCESSORS); j++)
    {
      *((router->flags)[j]) |= ~(1 << 11);
    }
  }

  /* Finally, reset the bit to zero by default - this simulates a hardware wire that will be zero
   * unless actually being asserted
   */

  for (uint32_t j = 0; j < (1 << PROCESSORS); j++) *((router->flags)[j]) &= ~(1 << 10);
}

/* To deliver messages we can do a similar thing to sending them, except now we only care about the
 * payload. Each iteration, we iterate over all 7 messages and deliver the appropiate bit to the right
 * processor when the address is 0.
 *
 * Notably the connection machine provides 2 methods for sending multiple messages to processors -
 * just oring them together or delivering one in each cycle. To allow this to work, The algorithm will
 * be to iterate backwards over the messages, to set or or in the bit as required. Then, on parity bit
 * delivery, delete the earliest occurence of the message if individual delivering, or all if or
 * delivering (not that the parity bit has much meaning in that case!)
 */

void router_deliver(Router *router, uint16_t bit, uint8_t shouldOr)
{

  uint32_t i;
  uint16_t deliverBits[1 << PROCESSORS];
  for (i = 0; i < (1 << PROCESSORS); i++) deliverBits[i] = 0; /* Initialise the array to zeroes, deliver
                                                               * 0 by default!
                                                               */
  /* Extract the processor address mask. This is the number of processors - 1. Special case if there
  * are 2**32 procs per router, need to set mask definitively to prevent underflow.
  */
  uint32_t procMask;
  if (PROCESSORS == 32)
  {
    procMask = -1;
  }
  else
  {
    procMask = (1 << PROCESSORS) -1;
  }

  /* Now once again distinct on the bit number. Send a 1 first as a type of handshake, if there's a
   * message for the processor.
   */
  if (bit == 0)
  {
    for (i = 0; i < BUFSIZE; i++)
    {
      if (router->buffer[i] == NULL) {/*printf("%u", i);*/ break;}
      if (router->buffer[i]->address >> PROCESSORS != 0) continue; /* Message for different router */
      deliverBits[router->buffer[i]->address & procMask] = 1;
    }
  }
  /* For bits then up to the length of the message, we can just read that bit - 1 of the message and
   * write it. Decreasing for loop so that, if shouldOr is false, the earliest message in the buffer
   * gets written.
   */
  else if (bit <= (MESSAGE_LENGTH << 3))
  {
    for (i = 0; i < BUFSIZE; i++)
    {
      if (router->buffer[(BUFSIZE - 1) -i] == NULL) continue;
      if (router->buffer[(BUFSIZE - 1) -i]->address >> PROCESSORS != 0) continue;
      /* 6-i is populated. Extract the bit */
      uint8_t msgVal = router->buffer[BUFSIZE-1-i]->message[(bit - 1) >> 3];
      msgVal = (msgVal >> (7 - ((bit - 1) & 7))) & 1; /* This is the actual bit now! */

      /* Now how we write it depends on the mode */
      if (shouldOr) deliverBits[(router->buffer[BUFSIZE - 1 -i]->address) & procMask] |= msgVal;
      else deliverBits[(router->buffer[BUFSIZE-1-i]->address) & procMask] = msgVal;
    }
  }
  /* Finally, the case where it's the parity bit. Here, just worry about delivering the bit - we'll free
   * the messages and sort out the buffer after delivering so we can reuse deliverBits if necessary!
   */
  else if (bit == (MESSAGE_LENGTH << 3) + 1)
  {
    //printf("cringe\n");
    for (i = 0; i < BUFSIZE; i++)
    {
      if (router->buffer[BUFSIZE-1-i] == NULL) continue;
      if (router->buffer[BUFSIZE-1-i]->address >> PROCESSORS != 0) continue;
      uint8_t msgVal = router->buffer[BUFSIZE-1-i]->parity;
      //printf("beans\n");

      if (shouldOr) deliverBits[router->buffer[BUFSIZE-1-i]->address & procMask] |= msgVal;
      else deliverBits[router->buffer[BUFSIZE-1-i]->address & procMask] = msgVal;
      //I'm lazy and not using the parity bit anyway.
      deliverBits[router->buffer[BUFSIZE-1-i]->address & procMask] = 0;
    }
  }

  /* Now the bits can actually be delivered to the appropiate processors. */
  for (i = 0; i < (1 << PROCESSORS); i++)
  {
    if (deliverBits[i]) /* == 1 */
    {
      *(router->flags[i]) |= 1 << 10;
    }
    else /* == 0 */
    {
      *(router->flags[i]) &= ~(1 << 10);
    }
  }

  /* Finally, if it's the parity bit, delete all the spent messages and fix the buffer */
  if (bit == (MESSAGE_LENGTH << 3) + 1)
  {
    /* If we're in or mode deleting is easy - just delete any msg with router address 0 */
    if (shouldOr)
    {
      for (i = 0; i < BUFSIZE; i++)
      {
        if ((router->buffer[i]->address) >> PROCESSORS == 0)
        {
          free(router->buffer[i]);
          router->buffer[i] = NULL;
        }
      }
    }
    /* Otherwise, this takes a bit more thinking. We reuse the deliverBits array, initially setting
     * everything to 1, then setting to 0 when a message with that processor is freed
     */
    else
    {
      for (i = 0; i < (1 << PROCESSORS); i++) deliverBits[i] = 1;
      for (i = 0; i < BUFSIZE; i++)
      {
	if (router->buffer[i] == NULL) continue;
        if (((router->buffer[i]->address) >> PROCESSORS == 0)
          && deliverBits[router->buffer[i]->address & procMask])
        {
	  //printf("\nGot here %u\n", i);
          deliverBits[router->buffer[i]->address & procMask] = 0;
          free(router->buffer[i]);
          router->buffer[i] = NULL;
        }
      }
    }
    /* Now we can shift the buffer down to remove the NULLs */
    uint8_t j;
    for (i = 0; i < BUFSIZE; i++)
    {
      if (router->buffer[i] == NULL)
      {
        for (j = i; j < BUFSIZE-1; j++) router->buffer[j] = router->buffer[j+1];
        router->buffer[BUFSIZE-1] = NULL;
      }
    }
  }

  /* And that's delivery done! */
}

int router_empty(Router *router)
{
  uint32_t i;
  for (i = 0; i < BUFSIZE; i++)
  {
    if (router->buffer[i] != NULL) return 1;
  }
  return 0;
}
