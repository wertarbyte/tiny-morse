#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>

#define MORSE_CLOCK_MS 250

#define MORSE_DDR  DDRD
#define MORSE_PORT PORTD
#define MORSE_BIT  PD0

static const char msg[] = "SOS";

#define ELEMS(x) (sizeof(x)/sizeof((x)[0]))
#include "codes.h"

static void wait(int ms) {
	while (ms--) _delay_ms(1);
}

static void flash(unsigned int ms) {
	MORSE_PORT |= (1<<MORSE_BIT);
	wait(ms);
	MORSE_PORT &= ~(1<<MORSE_BIT);
}

// buffer used to store morse code sequences retrieved from flash
static char buf[10] = {0};
static const char *lookup_char(char c) {
	for (uint8_t i=0; i<ELEMS(codes); i++) {
		if (codes[i].c == c) {
			return strcpy_P(buf, codes[i].seq);
		}
	}
	buf[0] = 0;
	return buf;
}

static void morse_char(char c) {
	const char *seq = lookup_char(c);
	while (*seq) {
		if (*seq == '-') {
			flash(MORSE_CLOCK_MS*2);
		} else if (*seq == '.') {
			flash(MORSE_CLOCK_MS*1);
		}
		wait(MORSE_CLOCK_MS);
		seq++;
	}
}

static void morse(void) {
	const char *m = msg;
	while ( *m ) {
		morse_char(*m);
		wait(MORSE_CLOCK_MS*2);
		m++;
	}
	wait(MORSE_CLOCK_MS*3);
}

int main(void) {
	MORSE_DDR |= 1<<MORSE_BIT;

	while(1) {
		morse();
	}
	return 0;
}
