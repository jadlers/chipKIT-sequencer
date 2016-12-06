#include <pic32mx.h>
#include "init.h"

#define COLUMNS 32
#define ROWS 128



const int NOTE_ON_MAX = 10;
int current_column = 0;
int time_counter = 0;		// Amount of 1/100-seconds from beginning of the loop
int beat_length;				// Amount of 1/100-seconds per beat (changed with potentiometer)
int play = 1;						// Send MIDI from matrix
int btns = 0;						// Stores pushbutton data for polling

/* struct for MIDI messages */
struct message {
	unsigned char command;
	unsigned char note;
	unsigned char velocity;
};

struct message messages[COLUMNS][ROWS];		// Matrix storing MIDI messages
unsigned char column_lengths[COLUMNS];		// Number of messages stored in each column

void save_message(struct message msg) {
	int save_column = current_column;

	if (time_counter > beat_length / 2) {
		save_column = (save_column + 1) % COLUMNS; // Round to nearest column
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
		if (cmd != 0x90 && cmd != 0x80) {
			IFSCLR(0) = 1 << 27;	// Clear interrupt flag
			return;
		}
		unsigned char note = U1RXREG & 0xFF;
		if (note >> 7) {
			IFSCLR(0) = 1 << 27;	// Clear interrupt flag
			return;
		}
		unsigned char vel = U1RXREG & 0xFF;
		IFSCLR(0) = 1 << 27;	// Clear interrupt flag
		if (vel >> 7) {
			return;
		}
		struct message msg = {
			cmd,
			note,
			vel
		};

		save_message(msg);
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

void metronome() {
	while(U1STA & (1 << 9));	// Make sure the write buffer is not full
	U1TXREG = 0x90;
	while(U1STA & (1 << 9));	// Make sure the write buffer is not full
	U1TXREG = 100;
	while(U1STA & (1 << 9));	// Make sure the write buffer is not full
	U1TXREG = 50;
	while(U1STA & (1 << 9));	// Make sure the write buffer is not full
	U1TXREG = 0x80;
	while(U1STA & (1 << 9));	// Make sure the write buffer is not full
	U1TXREG = 100;
	while(U1STA & (1 << 9));	// Make sure the write buffer is not full
	U1TXREG = 0;
}

void all_notes_off() {
	int i;
	for (i = 0; i < 128; i++) {
		while(U1STA & (1 << 9));	// Make sure the write buffer is not full
		U1TXREG = 0x80;
		while(U1STA & (1 << 9));	// Make sure the write buffer is not full
		U1TXREG = i;
		while(U1STA & (1 << 9));	// Make sure the write buffer is not full
		U1TXREG = 0;
	}
}

void clear_column_lengths() {
	int i;
	for (i = 0; i < COLUMNS; i++) {
		column_lengths[i] = 0;
	}
}

int main(void) {
	quicksleep(10000000);
	init();
	clear_column_lengths();


	T2CON |= 0x8000;		// Timer on

	for (;;) {

		if (time_counter > beat_length) {
			/* Start sampling potentiometer, wait until conversion is done */
			AD1CON1 |= (0x1 << 1);
			while(!(AD1CON1 & (0x1 << 1)));
			while(!(AD1CON1 & 0x1));

			/* Get the analog value and update beat_length */
			unsigned int value = (ADC1BUF0 >> 5);
			beat_length = 32 - value;

			PORTE = ~PORTE; // Flash the current tempo on the LEDs

			/* Increment the current column and wrap around at end of matrix */
			if (++current_column == COLUMNS) {
				current_column = 0;
			}

			/* For loop to go trough all rows in current column */

			if (current_column % 4 == 0) {
				metronome();
			}
			int i;
			for (i = 0; i < column_lengths[current_column]; i++) {
				struct message msg = messages[current_column][i];
				/* Send MIDI message */
				while(U1STA & (1 << 9));	// Make sure the write buffer is not full
				U1TXREG = msg.command;
				while(U1STA & (1 << 9));	// Make sure the write buffer is not full
				U1TXREG = msg.note;
				while(U1STA & (1 << 9));	// Make sure the write buffer is not full
				U1TXREG = msg.velocity;
			}

			time_counter = 0;
		}
		// display_string(3, itoaconv(btns));
		// display_update();
		int new_btns = get_btns();
		if (!(btns & 1) && (new_btns & 1)) {		// Button has been pressed
			if (play) {
				play = 0;
				T2CON &= ~0x8000;		// Timer off
				all_notes_off();
			} else {
				play = 1;
				T2CON |= 0x8000;		// Timer on
			}
		}
		if (!(btns >> 2 & 1) && (new_btns >> 2 & 1)) {
			clear_column_lengths();
			all_notes_off();
		}
		btns = new_btns;
	}

	return 0;
}
