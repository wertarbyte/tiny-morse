MCU = attiny2313
F_CPU = 1000000
TARGET = tiny-morse

include avr-tmpl.mk

codes.h: codes.txt
	awk -f codes.awk $< > $@


