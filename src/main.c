#include <pic32mx.h>
#include "init.h"

int array_pos_write = 0;
int array_pos_read = 0;
int time_counter = 0;		// Amount of 1/100-seconds from beginning of the loop
int beat_length = 12;		// Amount of 1/100-seconds per beat

/* struct for MIDI messages */
struct message {
	unsigned char command;
	unsigned char note;
	unsigned char velocity;
};

struct message messages[16];	// array with 10 MIDI messages for testing

/* Interrupt Service Routine */
void user_isr( void ) {
	/* MIDI receive interrupt */
	if (IFS(0) & (1 << 27)) {
		unsigned char cmd = U1RXREG & 0xFF;
		unsigned char note = U1RXREG & 0xFF;
		unsigned char vel = U1RXREG & 0xFF;
		struct message mes = {
			cmd,
			note,
			vel
		};

		messages[array_pos_write] = mes;	// Add message struct to array
		PORTE = note;											// Display played note on LEDs
		display_string(0, itoaconv(array_pos_write));
		display_update();

		if(++array_pos_write == sizeof(messages)/sizeof(struct message)) {
			array_pos_write = 0;
		}

		IFSCLR(0) = 1 << 27;	// Clear interrupt flag
	}
	/* Timer2 interupt */
	if (IFS(0) & (1 << 8)) {
		time_counter++;
		TMR2 = 0;						// Clear Timer2 counter
		IFSCLR(0) = 1 << 8;	// Clear interupt flag
	}
}

int calculate_baudrate_divider(int sysclk, int baudrate, int highspeed) {
	int pbclk, uxbrg, divmult;
	unsigned int pbdiv;

	divmult = (highspeed) ? 4 : 16;
	/* Periphial Bus Clock is divided by PBDIV in OSCCON */
	pbdiv = (OSCCON & 0x180000) >> 19;
	pbclk = sysclk >> pbdiv;

	/* Multiply by two, this way we can round the divider up if needed */
	uxbrg = ((pbclk * 2) / (divmult * baudrate)) - 2;
	/* We'll get closer if we round up */
	if (uxbrg & 1)
		uxbrg >>= 1, uxbrg++;
	else
		uxbrg >>= 1;
	return uxbrg;
}

int get_sw( void ) {
   return ((PORTD & (0xF << 8)) >> 8);
}

int get_btns(void) {
   return ((PORTD & (7 << 5)) >> 5);
}

char* char2str(char c) {
	static char arr[2];
	arr[0] = c;
	arr[1] = '\0';
	return arr;
}

/* Display info about a MIDI message */
void display_midi_info(struct message m) {
	/* Command */
	unsigned char command_check = (m.command & 0xF0) >> 4;
	if (command_check == 0x8) {
		display_string(1, "Note Off");
	} else if (command_check == 0x9) {
		display_string(1, "Note On");
	} else {
		display_string(1, "Something else");
	}

	/* Note */
	unsigned char note_check = m.note;
	display_string(2, itoaconv(note_check));

	/* Velocity */
	unsigned char velocity_check = m.velocity;
	display_string(3, itoaconv(velocity_check));
	display_update();
}

int main(void) {
	quicksleep(10000000);
	init();


	for (;;) {
		// display_string(1, itoaconv(time_counter));
		// display_update();
		if (time_counter > beat_length) {
			struct message msg = messages[array_pos_read];
			/* Send MIDI message */
			while(U1STA & (1 << 9));	// Make sure the write buffer is not full
			U1TXREG = msg.command;
			U1TXREG = msg.note;
			U1TXREG = msg.velocity;

			if (++array_pos_read == sizeof(messages)/sizeof(struct message)) {
				array_pos_read = 0;
				// turn_off_all_notes();	// TODO temporary fix, find other solution
			}

			display_midi_info(msg); 		// Display info about the send message
			time_counter = 0;
		}

		int btns = get_btns();
		if (btns & 1) {
			T2CON |= 0x8000;		// Timer on
		} else if (btns & 2) {
			T2CON &= ~0x08000;	// Timer off
		}

	}

	return 0;
}
