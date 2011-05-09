#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#define MORSE_CLOCK_MS 250

#define LED_DDR	DDRB
#define LED_OUT PORTB
#define LED_BIT	PB3

#define TRIGGER_DDR DDRB
#define TRIGGER_IN PINB
#define TRIGGER_BIT PB0

#define PADDLE_DDR DDRB
#define PADDLE_IN PINB
#define PADDLE_BIT PB4

#define EEPROM_LOC_USE_PREAMBLE	0
#define EEPROM_LOC_MSG 1

#define ELEMS(x) (sizeof(x)/sizeof((x)[0]))

struct sequence {
	// number of morse code symbols
	uint8_t length;
	// bitmask of the code, 1=DIT, 0=DAH
	uint8_t code;
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


static volatile struct {
	volatile unsigned long time_ms;
} clock = { 0 };

static struct {
	struct sequence symbols;
	char letters[8];
} recv_buffer = {{0,0}, {0}};

static void wait(int ms) {
	while (ms--) _delay_ms(1);
}

static void flash(unsigned int ms) {
	LED_OUT |= (1<<LED_BIT);
	wait(ms);
	LED_OUT &= ~(1<<LED_BIT);
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

static void append_letter(char l) {
	char *ptr = recv_buffer.letters;
	while (*ptr) ptr++;
	*ptr = l;
	*(ptr+1) = 0;
}

static void process_buffer(uint8_t paddle_was_pressed) {
	// a paddle state just ended, let's see what
	// we can make of that...
	unsigned long duration = clock.time_ms;
	clock.time_ms = 0;
	if (paddle_was_pressed) {
		recv_buffer.symbols.code <<= 1;
		recv_buffer.symbols.length++;
		if (duration >= TIME_DAH) {
			// add a DAH symbol to the receive buffer
		} else {
			// add a DIT symbol
			recv_buffer.symbols.code |= 1;
		}
	} else {
		// the paddle is pressed again, ending a pause. How long was it?
		if (duration >= PAUSE_LETTER && recv_buffer.symbols.length > 0) {
			// a new letter has been started, flush the buffer
			// and transform the read symbols into a letter
			char l = lookup_sequence(recv_buffer.symbols);
			if (l) {
				append_letter(l);
			}
			recv_buffer.symbols.code = 0;
			recv_buffer.symbols.length = 0;
		}
		if (duration >= PAUSE_WORD && recv_buffer.letters[0]) {
			append_letter(' ');
		}
	}
}

static uint8_t buffer_filled(void) {
	return (recv_buffer.letters[0] || recv_buffer.symbols.length);
}

static void receiver_timeout(void) {
	// process any tokens left
	process_buffer(0);
	// morse back the sequence
	morse_string(recv_buffer.letters);
	recv_buffer.letters[0] = 0;
}

int main(void) {
	LED_DDR |= (1<<LED_BIT);
	TRIGGER_DDR &= ~(1<<TRIGGER_BIT);
	PADDLE_DDR &= ~(1<<PADDLE_BIT);

	// configure TIMER1 with /1 prescaler and increment timer_ms every 5 overflows (-> ~0.001s)
	TCCR0B |= (1<<CS00);
	TIMSK0 |= (1<<TOIE0);

	sei();

	uint8_t paddle_state = 0;
	while(1) {
		/*
		if (TRIGGER_IN & 1<<TRIGGER_BIT) {
			morse();
		}
		*/
		uint8_t paddle_now = (PADDLE_IN & 1<<PADDLE_BIT);
		if (paddle_now != paddle_state) {
			// paddle state has just changed
			wait(3); // debounce the simple way
			// handle tokens stored in buffer
			process_buffer(paddle_state);
		}
		paddle_state = paddle_now;
		// receiver timeout
		if (clock.time_ms > 10*(MORSE_CLOCK_MS) && buffer_filled()) {
			receiver_timeout();
			clock.time_ms = 0;
		}
	}
	return 0;
}

SIGNAL(TIM0_OVF_vect) {
	static uint8_t overflows = 5;
	if (overflows-- == 0) {
		clock.time_ms++;
		overflows = 5;
	}
}

