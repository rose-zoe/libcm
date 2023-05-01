#ifndef CM_ROUTER_H_
#define CM_ROUTER_H_

#include <stdint.h>

#define DIMENSIONS 12
#define PROCESSORS  4 /* log_2 of the number of processors associated with 1 router */
#define MESSAGE_LENGTH 4 /* message length in bytes*/
#define ADDRLEN (DIMENSIONS + PROCESSORS)
#define BUFSIZE 7

typedef struct
{
  uint32_t address; /* least sig PROCESSORS bits are within a router, next least sig DIM bits for router */
  uint8_t message[MESSAGE_LENGTH];
  uint8_t parity;
} Message;

typedef struct rint
{
  Message *inports[DIMENSIONS];
  Message **outports[DIMENSIONS];
  Message *buffer[BUFSIZE];
  uint32_t listening[4];
  Message *partials[4];
  uint16_t *flags[1 << PROCESSORS];
  struct rint *referer;
  uint32_t id;
} Router;

void router_forward(Router *router, uint32_t dimension);

void router_inject(Router *router, uint16_t bit);

void router_deliver(Router *router, uint16_t bit, uint8_t shouldOr);

void router_receive(Router *router, uint32_t dim);

int router_empty(Router *router);

#endif
