MCU = attiny13
F_CPU = 1000000
TARGET = tiny-morse

include avr-tmpl.mk

codes.h: codes.txt codes.awk
	awk -f codes.awk $< > $@


