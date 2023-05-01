# libcm
A cycle accurate simulator of the Connection Machine (CM-1) architecture, provided as a C library

## A Brief History
The Connection Machine was a supercomputer designed by Dnaile Hillis for his PhD thesis in 1985.
Apparently only 7 were actually built. This code `libcm` acts as a cycle accurate simulator for the
Connection Machine for the purposes of preservation and evaluation

## How Do I Connect the Machine?
The C code can be compiled using gcc, and the `connection_machine.h` header imported into your programs.
By writing in calls to Connection Machine functions, you can simulate the Machine running on code

## CMFrames?
CMFrames is a simple Python script that can analyse dumps from the Connection Machine to find program
bugs. It is very fragile, but powerful and useful in a lot of circumstances.

## libcm Function Breakdown
All these are the functions included in connection_machine.h

### cm_build
`cm *cm_build()`

Assigns memory space for and returns a pointer to a simulated Connection Machine.

### cm_del
`void cm_del(cm *machine)`

Deletes a simulated Connection Machine.

### cm_exe
`void cm_exe(cm *machine, uint16_t addrA, uint16_t addrB, uint8_t flagR, uint8_t flagW, uint8_t flagC, uint8_t sense, uint8_t memTruth, uint8_t flagTruth, uint8_t newsDir)`

The main function to interact with the Machine. Allows issuing of instructions to the Machine for
simulation. Details of the instruction set are explained in Hillis's book.

### shouldOr & shouldntOr
`uint8_t shouldOr(cm *machine)`

`uint8_t shouldntOr(cm *machine)`

Toggles the simulated Machine between OR and HIPRI router modes. In OR, every message received for a cell
on a message cycle will be ORed together and delivered, whereas in HIPRI only the highest priority one
will and the rest will remain in the router until the next cycle. Returns 0 on success, fails and returns
-1 if the message cycle isn't synchronised (can only be called at the start of a message cylce).

### slowMode & fastMode
`uint8_t slowMode(cm *machine)`

`uint8_t fastMode(cm *machine)`

Toggles the simulated Machines router speed. In slowMode, the routers will deliver messages at true
simulated speed, taking ~700 cycles to deliver a message. In fastMode, this is greatly sped up to ~100
cycles per message, reducing the size of program dumps and time taken for test programs. Note that
fastMode will break certain programs that do things during the router waiting period. Returns 0 on
success, fails and returns -1 if the message cycle isn't synchronised (can only be called at the start of
a message cylce).

### shouldDump & shouldntDump
`uint8_t shouldDump(cm *machine)`

`uint8_t shouldntDump(cm *machine)`

Toggles whether the machine should dump its entire state at every cycle to a dump.dat compressed archive
that can be analysed with CMFrames. Returns 0 on success, fails and returns -1 if the simulation has
already started.

### petit_sync
`void petit_sync(cm *machine)`

Repeatedly does no-ops until the router ("petit") cycle is synchronised, at which point cells can begin
sending messages. Very useful in any program that uses message passing.

### network_empty
`int network_empty(cm *machine)`

Tests if the network contains messages to be delivered. Counterintuitively returns 1 if there are any
messages left, and 0 if the network is empty.

### cycles
`void cycles()`

Prints out the total number of calls to cm_exe. Useful in judging how long a program took to run.
