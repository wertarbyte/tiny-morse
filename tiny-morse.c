#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

#define MORSE_CLOCK_MS 250

#define LED_DDR	DDRB
#define LED_PORT PORTB
#define LED_BIT	PB3

#define TRIGGER_DDR DDRB
#define TRIGGER_PORT PINB
#define TRIGGER_BIT PB0

#define EEPROM_LOC_USE_PREAMBLE	0
#define EEPROM_LOC_MSG 1

#define ELEMS(x) (sizeof(x)/sizeof((x)[0]))

struct sequence {
	// number of morse code symbols
	const uint8_t length;
	// bitmask of the code, 1=DIT, 0=DAH
	const uint8_t code;
};

static const struct sequence CODE_EMPTY    = { 0, 0 };
static const struct sequence CODE_STARTMSG = { 5, 0b10101 };
static const struct sequence CODE_ENDMSG   = { 5, 0b01010 };

#include "codes.h"

// morse code durations
#define TIME_DIT MORSE_CLOCK_MS
#define TIME_DAH (TIME_DIT*3)
#define PAUSE_SYMBOL TIME_DIT
#define PAUSE_LETTER TIME_DAH
#define PAUSE_WORD (TIME_DIT*7)

static void wait(int ms) {
	while (ms--) _delay_ms(1);
}

static void flash(unsigned int ms) {
	LED_PORT |= (1<<LED_BIT);
	wait(ms);
	LED_PORT &= ~(1<<LED_BIT);
}

static const struct sequence lookup_char(char c) {
	struct sequence buffer;
	int i = c-'!';
	if (i < ELEMS(codes)) {
		memcpy_P( &buffer, &codes[i], sizeof(struct sequence) );
		return buffer;
	}
	return CODE_EMPTY;
}

static const char lookup_sequence(struct sequence s) {
	struct sequence buffer;
	for (int i=0; i<ELEMS(codes); i++) {
		memcpy_P( &buffer, &codes[i], sizeof(struct sequence) );
		if (buffer.length == s.length && buffer.code == s.code) {
			return (char)(i+'!');
		}
	}
	return 0;
}

static void morse_sequence(struct sequence s) {
	uint8_t l = s.length;
	while (l > 0) {
		if (s.code & 1<<(l-1)) {
			flash(TIME_DIT);
		} else {
			flash(TIME_DAH);
		}
		wait(PAUSE_SYMBOL);
		l--;
	}
}

static void morse_char(char c) {
	if (c != ' ') {
		const struct sequence s = lookup_char(c);
		morse_sequence(s);
	} else {
		wait(PAUSE_WORD);
	}
}

static void morse_string(const char *str) {
	while ( *str ) {
		morse_char(*str);
		wait(PAUSE_LETTER);
		str++;
	}
}

static void morse_eeprom(void) {
	uint8_t *i = (uint8_t*) EEPROM_LOC_MSG;
	char m = 0;
	while ( (m = eeprom_read_byte(i++)) != 0 ) {
		if (m == '\n') break;
		morse_char(m);
		wait(PAUSE_LETTER);
	}
}

static uint8_t morse_preamble(void) {
	return eeprom_read_byte(EEPROM_LOC_USE_PREAMBLE) == '1';
}

static void morse(void) {
	uint8_t preamble = morse_preamble();
	if (preamble) {
		morse_sequence( CODE_STARTMSG );
		wait(TIME_DAH*2);
	}

	morse_eeprom();

	if (preamble) {
		wait(TIME_DAH*2);
		morse_sequence( CODE_ENDMSG );
	}
}

int main(void) {
	LED_DDR |= 1<<LED_BIT;
	TRIGGER_DDR |= 1<<TRIGGER_BIT;

	while(1) {
		if (TRIGGER_PORT & 1<<TRIGGER_BIT) {
			morse();
		}
	}
	return 0;
}
