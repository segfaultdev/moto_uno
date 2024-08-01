#!/bin/bash

sdas6808 -o -z strap.asm
sdld6808 -i -b _CODE=0x00AC strap.rel
sdobjcopy -I ihex -O binary strap.ihx strap.bin

rm strap.rel strap.ihx
xxd -i strap.bin
