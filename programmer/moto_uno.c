#define F_CPU 16000000
#define BAUD 19200

#include <util/setbaud.h>
#include <util/delay.h>
#include <avr/io.h>

#include <stdint.h>
#include <stdio.h>

// Pin layout:
// - PD2 => Power enable (GATE_5V and GATE_9V)
// - PD3 => 4 MHz clock
// - PD4 => UART data
// - PD5 => Reset

// *** Begin STRAP.ASM ***

unsigned char strap_bin[] = {
	0x6e, 0x04, 0x89, 0x6e, 0x40, 0x88, 0x45, 0xf8, 0x00, 0xcd, 0x28, 0x06,
	0x10, 0x05, 0x6e, 0x32, 0x84, 0x45, 0xf8, 0x1f, 0x35, 0x8a, 0x10, 0x01,
	0x45, 0x00, 0x8c, 0xcd, 0x28, 0x00, 0xf7, 0x5c, 0xa3, 0xac, 0x25, 0xf7,
	0x6e, 0x04, 0x89, 0x55, 0x8a, 0xb6, 0x84, 0xa1, 0x02, 0x26, 0x05, 0x45,
	0xff, 0xdf, 0x35, 0x8a, 0xaf, 0xe1, 0x11, 0x01, 0x89, 0x8b, 0xcd, 0x28,
	0x09, 0x8a, 0x88, 0xaf, 0x3f, 0x35, 0x8a, 0x3b, 0x84, 0xd0, 0x8e,
};

int strap_bin_len = 71;

// *** End STRAP.ASM ***

uint8_t v_code[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

char serial_getchar(FILE *stream) {
	loop_until_bit_is_set(UCSR0A, RXC0);
	return UDR0;
}

void serial_putchar(char c, FILE *stream) {
	if (c == '\n') {
		serial_putchar('\r', stream);
	}
	
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
}

static inline void serial_enable(void) {
	UBRR0H = UBRRH_VALUE;
	UBRR0L = UBRRL_VALUE;
	
#if USE_2X
	UCSR0A |= _BV(U2X0);
#else
	UCSR0A &= ~(_BV(U2X0));
#endif
	
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
	UCSR0B = _BV(RXEN0) | _BV(TXEN0);
	
	static FILE serial_input = FDEV_SETUP_STREAM(NULL, serial_getchar, _FDEV_SETUP_READ);
	stdin = &serial_input;
	
	static FILE serial_output = FDEV_SETUP_STREAM(serial_putchar, NULL, _FDEV_SETUP_WRITE);
	stdout = &serial_output;
}

static inline void moto_disable(void) {
	TCCR2A = _BV(WGM21);
	TCCR2B = _BV(CS20);
	OCR2A = 0x01;
	
	PORTD = 0x00;
	DDRD = 0x0C;
	
	// Ensure the microcontroller has no power.
	
	_delay_ms(500);
}

static inline void moto_enable(void) {
	PORTD = 0x04;
	DDRD = 0x0C;
	
	TCCR2A = _BV(COM2B0) | _BV(WGM21);
	TCCR2B = _BV(CS20);
	OCR2A = 0x01;
	
	// Delay for 4128, 4 MHz ticks.
	// Delay for 256, 1 MHz ticks (1024, 4 MHz ticks).
	
	_delay_us(1288);
}

static inline int moto_available(void) {
	return !(PIND & 0x10);
}

static inline uint8_t moto_receive(void) {
	uint8_t x = 0x00;
	while (PIND & 0x10);
	
	for (int i = -1; i < 9; i++) {
		if (i >= 0 && i <= 7) {
			x |= (((PIND & 0x10) >> 4) << i);
		}
		
		_delay_us(256);
	}
	
	return x;
}

static inline void moto_send_no_echo(uint8_t x) {
	uint16_t y = ((uint16_t)(x) << 1) | 0x0200;
	
	while (y) {
		DDRD = (DDRD & 0xEF) | (((~y) & 1) << 4);
		y >>= 1;
		
		_delay_us(256);
	}
}

static inline void moto_send(uint8_t x) {
	moto_send_no_echo(x);
	uint8_t echo_x = moto_receive();
	
	if (x != echo_x) {
		printf("[FAIL] send(0x%02X) != echo(0x%02X)\n", x, echo_x);
		for (;;);
	}
	
	_delay_us(1024);
}

static inline void moto_verify(void) {
	for (int i = 0; i < 8; i++) {
		moto_send(v_code[i]); // ((0x11 * i) ^ 0xCA);
	}
	
	uint8_t x = moto_receive();
	
	if (x != 0x00) {
		printf("[FAIL] receive(0x%02X) != 0x00\n", x);
		for (;;);
	}
}

static inline void moto_command_write(uint16_t address, uint8_t x) {
	moto_send(0x49);
	
	moto_send((uint8_t)(address >> 8));
	moto_send((uint8_t)(address >> 0));
	
	moto_send(x);
	_delay_us(15360);
}

int main(void) {
	moto_disable();
	serial_enable();
	
	moto_enable();
	moto_verify();
	
	uint16_t address, count;
	uint8_t x;
	
	char c;
	
	for (;;) {
		printf("$ ");
		scanf("%c", &c);
		
		if (c == 'r' || c == 'R') {
			scanf("%04X %04X", &address, &count);
			putchar('\n');
			
			for (uint16_t i = 0; i < count; i++) {
				if ((uint8_t)(i & 0x000F) == 0x00) {
					printf("%04X = ", address);
				}
				
				moto_send(0x4A);
				
				moto_send((uint8_t)(address >> 8));
				moto_send((uint8_t)(address >> 0));
				
				x = moto_receive();
				address++;
				
				printf("%02X%c", x, ((uint8_t)(i & 0x000F) < 0x0F && i + 1 < count) ? ' ' : '\n');
			}
		}
		
		if (c == 'w' || c == 'W') {
			scanf("%04X %02X", &address, &x);
			putchar('\n');
			
			moto_command_write(address, x);
		}
		
		if (c == 'b' || c == 'B') {
			putchar('\n');
			
			for (int i = 0; i < strap_bin_len; i++) {
				moto_command_write(0x00AC + i, strap_bin[i]);
			}
			
			moto_command_write(0x00FE, 0x00);
			moto_command_write(0x00FF, 0xAC);
		}
		
		if (c == 's' || c == 'S') {
			putchar('\n');
			
			moto_send(0x0C);
			
			address = ((uint16_t)(moto_receive()) << 8);
			address |= (uint16_t)(moto_receive());
			
			printf("%04X\n", address);
		}
		
		if (c == 'x' || c == 'X') {
			putchar('\n');
			
			moto_send(0x28);
		}
		
		if (c == 'p' || c == 'P') {
			putchar('\n');
			
			moto_disable();
			
			moto_enable();
			moto_verify();
		}
		
		if (c == 'v' || c == 'V') {
			for (int i = 0; i < 8; i++) {
				scanf("%02X", &x);
				v_code[i] = x;
			}
			
			putchar('\n');
		}
		
		if (c == 't' || c == 'T') {
			count = 0x0640;
			putchar('\n');
			
			while (count--) {
				scanf("%02X", &x);
				moto_send_no_echo(x);
			}
		}
	}
	
	return 0;
}
