#!/bin/env python

#This will work on a command based system
commands = """
rXXXX - select router x
cXX - select cell x in the current router
sr - show the state of the current router
sc - show the state of the current cell
d - advance 1 frame
dXX - advance many frames
a - back one frame
aXX - back many frames
x - exit
h - help
"""

from zipfile import ZipFile

filename = input("Enter file name for analysis: ")

with ZipFile(filename, "r") as ark:
  topFrame = len(ark.namelist()) - 1
  frame = 0
  router = 0
  cell = 0
  print("File loaded successfully. Contains ", len(ark.namelist()), " frames")
  while True:
    inp = input("[f: "+str(frame)+" r: "+str(router)+" c: "+str(cell)+"]$ ")
    if inp[0] == "r" and int(inp[1:]) < 4096:
      router = int(inp[1:])
    elif inp[0] == "c" and int(inp[1:]) < 16:
      cell = int(inp[1:])
    elif inp == "sc":
      #Each cell occupies 514 bytes
      with ark.open(str(frame), "r") as f:
        cellData = f.read()[8372 * router + cell*514: 8372 * router + (cell+1)*514]
      print("Flags: ", int(cellData[1]) * 256 + int(cellData[0]))
      print("Memory: ", list(int(cellData[i+2]) for i in range(512)))
    elif inp == "sr":
      with ark.open(str(frame), "r") as f:
        routerData = f.read()[8372 * router + 16*514: 8372 * (router+1)]
      #Messages are 9 bytes - a 4 byte address, a 4 byte message, and a parity bit byte. There's also
      #the 16 byte listening array at the FRONT of the router data
      for i in range(7):
        print("Message ", i, ":")
        print(" - Address: ", format(int(routerData[12*i + 1]) * 256 + int(routerData[12*i]), '#018b'))
        print(" - Message: ", list(int(routerData[12*i + 4 + j]) for j in range (4)))
        print(" - Parity: ", int(routerData[12*i + 8]))

      for i in range(4):
        print("Partial Message ", i, ":")
        print(" - Address: ", format(int(routerData[12*i + 101]) * 256 + int(routerData[12*i + 100]), '#018b'))
        print(" - Message: ", list(int(routerData[12*i + 104 + j]) for j in range (4)))
        print(" - Parity: ", int(routerData[12*i + 108]))
        print(" - Listener: ", int(routerData[84 + 4*i]))
    elif inp[0] == "d":
      if inp[1:] != "": av = int(inp[1:])
      else: av = 1
      frame = min(frame+av, topFrame)
    elif inp[0] == "a":
      if inp[1:] != "": av = int(inp[1:])
      else: av = 1
      frame = max(frame-av, 0);
    elif inp[0] == "x":
      break
    elif inp[0] == "h":
      print(commands)
    elif inp[0] == "i":
      print("Instruction:")
      with ark.open(str(frame), "r") as f:
        insData = f.read()[34291712 : 34291720]
      ins64 = 0
      for i in range(8):
        ins64 = ins64 << 8
        ins64 = ins64 | int(insData[7-i])
      print(" - addrA: ", (ins64 >> 43) & ((1 << 12) - 1))
      print(" - addrB: ", (ins64 >> 31) & ((1 << 12) - 1))
      print(" - flagR: ", (ins64 >> 27) & ((1 <<  4) - 1))
      print(" - flagW: ", (ins64 >> 23) & ((1 <<  4) - 1))
      print(" - flagC: ", (ins64 >> 19) & ((1 <<  4) - 1))
      print(" - sense: ", (ins64 >> 18) & ( 1           ))
      print(" - mTrut: ", (ins64 >> 10) & ((1 <<  8) - 1))
      print(" - fTrut: ", (ins64 >>  2) & ((1 <<  8) - 1))
      print(" - newsD: ", (ins64      ) & ((1 <<  2) - 1))
    else:
      print("Unknown command")
      print(commands)

