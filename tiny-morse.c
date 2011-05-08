#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

#define MORSE_CLOCK_MS 250

#define MORSE_DDR	DDRB
#define MORSE_PORT	PORTB
#define MORSE_BIT	PB3

#define TRIGGER_DDR	DDRB
#define TRIGGER_PORT	PINB
#define TRIGGER_BIT	PB0

#define EEPROM_MORSE_PREAMBLE	0
#define EEPROM_MSG_START	1

#define ELEMS(x) (sizeof(x)/sizeof((x)[0]))

struct sequence {
	// number of morse code symbols
	const uint8_t length;
	// bitmask of the code, 1=DIT, 0=DAH
	const uint8_t code;
};

struct morse {
	const char letter;
	const struct sequence seq;
};

static const struct sequence CODE_EMPTY    = { 0, 0 };
static const struct sequence CODE_STARTMSG = { 5, 0b10101 };
static const struct sequence CODE_ENDMSG   = { 5, 0b01010 };

#include "codes.h"

// morse code durations
#define MORSE_DIT MORSE_CLOCK_MS
#define MORSE_DAH ((MORSE_DIT)*3)
#define MORSE_SYMBOL_PAUSE ((MORSE_DIT))
#define MORSE_LETTER_PAUSE ((MORSE_DAH))
#define MORSE_WORD_PAUSE ((MORSE_DIT)*7)

static void wait(int ms) {
	while (ms--) _delay_ms(1);
}

static void flash(unsigned int ms) {
	MORSE_PORT |= (1<<MORSE_BIT);
	wait(ms);
	MORSE_PORT &= ~(1<<MORSE_BIT);
}

static const struct sequence lookup_char(char c) {
	struct morse buffer;
	for (int i=0; i<ELEMS(codes); i++) {
		memcpy_P( &buffer, &codes[i], sizeof(struct morse) );
		if (buffer.letter == c) {
			return buffer.seq;
		}
	}
	return CODE_EMPTY;
}

static void morse_sequence(struct sequence s) {
	uint8_t l = s.length;
	while (l > 0) {
		if (s.code & 1<<(l-1)) {
			flash(MORSE_DIT);
		} else {
			flash(MORSE_DAH);
		}
		wait(MORSE_SYMBOL_PAUSE);
		l--;
	}
}

static void morse_char(char c) {
	if (c != ' ') {
		const struct sequence s = lookup_char(c);
		morse_sequence(s);
	} else {
		wait(MORSE_WORD_PAUSE);
	}
}

static void morse_string(const char *str) {
	while ( *str ) {
		morse_char(*str);
		wait(MORSE_LETTER_PAUSE);
		str++;
	}
}

static void morse_eeprom(void) {
	uint8_t *i = (uint8_t*) EEPROM_MSG_START;
	char m = 0;
	while ( (m = eeprom_read_byte(i++)) != 0 ) {
		if (m == '\n') break;
		morse_char(m);
		wait(MORSE_LETTER_PAUSE);
	}
}

static uint8_t morse_preamble(void) {
	return eeprom_read_byte(EEPROM_MORSE_PREAMBLE) == '1';
}

static void morse(void) {
	uint8_t preamble = morse_preamble();
	if (preamble) {
		morse_sequence( CODE_STARTMSG );
		wait(MORSE_DAH*2);
	}

	morse_eeprom();

	if (preamble) {
		wait(MORSE_DAH*2);
		morse_sequence( CODE_ENDMSG );
	}
}

int main(void) {
	MORSE_DDR |= 1<<MORSE_BIT;
	TRIGGER_DDR |= 1<<TRIGGER_BIT;

	while(1) {
		if (TRIGGER_PORT & 1<<TRIGGER_BIT) {
			morse();
			wait(2000);
		}
	}
	return 0;
}
