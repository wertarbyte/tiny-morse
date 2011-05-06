#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

#define MORSE_CLOCK_MS 250

#define MORSE_DDR	DDRB
#define MORSE_PORT	PORTB
#define MORSE_BIT	PB3

#define EEPROM_MSG_START	0

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

static PGM_P lookup_char(char c) {
	for (int i=0; i<ELEMS(codes); i++) {
		PGM_P p;
		memcpy_P(&p, &codes[i], sizeof(PGM_P));
		if (pgm_read_byte(p) == c) {
			return p+1;
		}
	}
	return 0;
}

static void morse_sequence(PGM_P seq) {
	char s = 0;
	while (seq && (s = pgm_read_byte(seq))) {
		if (s == '-') {
			flash(MORSE_CLOCK_MS*3);
		} else if (s == '.') {
			flash(MORSE_CLOCK_MS*1);
		}
		wait(MORSE_CLOCK_MS);
		seq++;
	}
}

static void morse_char(char c) {
	PGM_P seq = lookup_char(c);
	morse_sequence(seq);
}

static void morse_string(const char *str) {
	while ( *str ) {
		morse_char(*str);
		str++;
	}
	wait(MORSE_CLOCK_MS*2);
}

static void morse_eeprom(void) {
	uint8_t i = EEPROM_MSG_START;
	char m = 0;
	while ( m = eeprom_read_byte(i++) ) {
		if (m == '\n') break;
		morse_char(m);
		wait(MORSE_CLOCK_MS*2);
	}
}

static void morse(void) {
	morse_sequence( CODE_STARTMSG );
	wait(500);
	morse_eeprom();
	wait(500);
	morse_sequence( CODE_ENDMSG );
}

int main(void) {
	MORSE_DDR |= 1<<MORSE_BIT;

	while(1) {
		morse();
		wait(2000);
	}
	return 0;
}
