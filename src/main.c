#include <pic32mx.h>
#include "init.h"

const int NOTE_ON_MAX = 10;
int COLUMNS;						// Kinda const
int ROWS;								// Kinda const
int current_column = 0;
int time_counter = 0;		// Amount of 1/100-seconds from beginning of the loop
int beat_length;				// Amount of 1/100-seconds per beat (changed with potentiometer)

/* struct for MIDI messages */
struct message {
	unsigned char command;
	unsigned char note;
	unsigned char velocity;
};

struct message messages[64][64];		// Matrix storing MIDI messages
unsigned char column_lengths[64];		// Number of messages stored in each column
unsigned char note_on_counters[64];	// Number of note on messages stored in each column

void save_message(struct message msg) {
	char note_on = ((msg.command & 0xF0) >> 4 == 0x9);	// 1 if command is note on, 0 otherwise
	int save_column = current_column;

	if (time_counter > beat_length / 2) {
		save_column = (save_column + 1) % COLUMNS; // Round to nearest column
	}

	if (note_on) {
		if (note_on_counters[save_column] >= NOTE_ON_MAX) {
			return; // Maximum number of allowed note on commands in column reached: Don't save
		}
		note_on_counters[save_column]++;
	}

	if (column_lengths[save_column] < ROWS) { // Make sure we don't overflow messages in save_column
		messages[save_column][column_lengths[save_column]] = msg;	// Add message struct to array
		column_lengths[save_column]++;
	}
}

/* Interrupt Service Routine */
void user_isr( void ) {
	/* MIDI receive interrupt */
	if (IFS(0) & (1 << 27)) {
		unsigned char cmd = U1RXREG & 0xFF;
		unsigned char note = U1RXREG & 0xFF;
		unsigned char vel = U1RXREG & 0xFF;
		struct message msg = {
			cmd,
			note,
			vel
		};

		save_message(msg);

		IFSCLR(0) = 1 << 27;	// Clear interrupt flag
	}
	/* Timer2 interupt */
	if (IFS(0) & (1 << 8)) {
		time_counter++;
		TMR2 = 0;						// Clear Timer2 counter
		IFSCLR(0) = 1 << 8;	// Clear interupt flag
	}
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

	/* Setup for arrays and matrix */
	ROWS = sizeof(messages[0]) / sizeof(struct message);
	COLUMNS = sizeof(messages) / (sizeof(struct message) * ROWS);

	// TODO move initialization of arrays to init.c
  int i;
  for (i = 0; i < COLUMNS; i++) {
		column_lengths[i] = 0;
		note_on_counters[i] = 0;
  }

	for (;;) {
		/* Start sampling potentiometer, wait until conversion is done */
		AD1CON1 |= (0x1 << 1);
		while(!(AD1CON1 & (0x1 << 1)));
		while(!(AD1CON1 & 0x1));

		/* Get the analog value and update beat_length */
		unsigned int value = (ADC1BUF0 >> 5);
		beat_length = 32 - value;
		display_string(3, itoaconv(beat_length));
		display_update();

		if (time_counter > beat_length) {
			PORTE = ~PORTE; // Flash the current tempo on the LEDs

			/* For loop to go trough all rows in current column */
			int i;
			for (i = 0; i < column_lengths[current_column]; i++) {
				struct message msg = messages[current_column][i];
				/* Send MIDI message */
				while(U1STA & (1 << 9));	// Make sure the write buffer is not full
				U1TXREG = msg.command;
				U1TXREG = msg.note;
				U1TXREG = msg.velocity;
			}

			/* Increment the current column and wrap around at end of matrix */
			if (++current_column == COLUMNS) {
				current_column = 0;
			}

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
