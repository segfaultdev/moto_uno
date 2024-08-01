#!/bin/bash

avr-gcc -Os -mmcu=atmega328p moto_uno.c -o moto_uno.elf
avr-objcopy -O ihex moto_uno.elf moto_uno.hex

avrdude -c arduino -p atmega328p -P $1 -U flash:w:moto_uno.hex
