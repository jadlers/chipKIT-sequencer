#include <pic32mx.h>
#include "init.h"

#define COLUMNS 32
#define ROWS 64
#define UNDO_LENGTH 15

int current_column = 0;
int time_counter = 0;		// Amount of 1/100-seconds from beginning of the loop
int beat_length;				// Amount of 1/100-seconds per beat (changed with potentiometer)
int play = 1;						// Send MIDI from matrix
int btns = 0;						// Stores pushbutton data for polling
int record = 0;					// 1 if recording is on, 0 oterwise
int undo_index = 0;			// Current undo step
int highest_note = 0;		// The highest note stored in the sequence
int lowest_note = 127;		// The lowest note stored in the sequence
int tempo_timer = 0;

/* struct for MIDI messages */
struct message {
	unsigned char command;
	unsigned char note;
	unsigned char velocity;
	unsigned char enable;
};

struct message messages[COLUMNS][ROWS];										// Matrix storing MIDI messages
unsigned char column_lengths[COLUMNS];										// Number of messages stored in each column
unsigned char prev_column_lengths[UNDO_LENGTH][COLUMNS];	// Stores copy of column_lengths for undo steps

/* Send MIDI message */
void send_midi_message(struct message msg) {
	while(U1STA & (1 << 9));	// Make sure the write buffer is not full
	U1TXREG = msg.command;
	while(U1STA & (1 << 9));	// Make sure the write buffer is not full
	U1TXREG = msg.note;
	while(U1STA & (1 << 9));	// Make sure the write buffer is not full
	U1TXREG = msg.velocity;
}

void save_message(struct message msg) {
	int save_column = current_column;

	if (time_counter > beat_length / 2) {
		save_column = (save_column + 1) % COLUMNS; // Round to nearest column
		msg.enable = 0;												 // Don't play the very next beat
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

		// Only save when record & play is enabled
		if (record && play) {
			struct message msg = {
				cmd,
				note,
				vel,
				1
			};

			if (note > highest_note) {
				highest_note = note;
			}
			if (note < lowest_note) {
				lowest_note = note;
			}

			save_message(msg);
		}
	}
	/* Timer2 interupt */
	if (IFS(0) & (1 << 8)) {
		time_counter++;
		tempo_timer++;
		TMR2 = 0;						// Clear Timer2 counter
		IFSCLR(0) = 1 << 8;	// Clear interupt flag
	}
}

// Return the state of all switches
int get_sw( void ) {
   return ((PORTD & (0xF << 8)) >> 8);
}

// Return all 4 pushbutton states
int get_btns(void) {
   return ((PORTD & (7 << 5)) >> 4) | (( PORTF & 2) >> 1);
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

// Sends a note on and off for the same note in the same beat
void metronome() {
	struct message note_on = {0x90, 100, 50, 0};
	struct message note_off = {0x80, 100, 0, 0};
	send_midi_message(note_on);
	send_midi_message(note_off);
}

// Sends note off messages for all notes 4 times
void all_notes_off() {
	int i, j;
	for (j = 0; j < 4; j++) {
		for (i = 0; i < 128; i++) {
			struct message msg = {0x80, i, 0, 0};
			send_midi_message(msg);
		}
	}
}

/*
Goes through the column played 2 beats ago and:
	- Removes duplicated note on messages for the same note in the same column.
	- If a note has both a note on and note off in the same column, move the note of
	  to the next column
*/
void fix_previous_column() {
	int cleanup_column = (current_column + COLUMNS - 2) % COLUMNS;
	int i;
	for (i = 0; i < column_lengths[cleanup_column]; i++) {
		struct message msg1 = messages[cleanup_column][i];
		if (msg1.command == 0x90) {
			int j;
			for (j = i+1; j < column_lengths[cleanup_column]; j++) {
				struct message msg2 = messages[cleanup_column][j];
				if (msg1.note == msg2.note) {
					if (msg2.command == 0x80) {
						// Move msg2 to next column
						int next_column = (cleanup_column + 1) % COLUMNS;
						messages[next_column][column_lengths[next_column]] = messages[cleanup_column][j];
						column_lengths[next_column]++;
					}
					// Put last message on place j and decrement column_lengths
					messages[cleanup_column][j] = messages[cleanup_column][column_lengths[cleanup_column] - 1];
					column_lengths[cleanup_column]--;
					j--;	// Compare msg1 with moved message next iteration
				}
			}
		}
	}
}

// Set all values in column_lengths to 0
void clear_column_array(unsigned char *arr) {
	int i;
	for (i = 0; i < COLUMNS; i++) {
		arr[i] = 0;
	}
}

int notes_recorded() {
	int i;
	for (i = 0; i < COLUMNS; i++) { // Check if column_lengths changed
		if (prev_column_lengths[undo_index][i] != column_lengths[i]) {
			return 1;
		}
	}
	return 0;
}

// Handles the functionallity for all buttons and switches
void handle_input() {
	int new_record = get_sw() & (1 << 2);
	int new_btns = get_btns();

	if (!(btns & 1) && (new_btns & 1)) {												// Transpose pushed down
		int transpose_up = get_sw() & 2;
		if (!(transpose_up && highest_note == 127) && !(!transpose_up && lowest_note == 0)) {
			if (transpose_up) {
				highest_note++;
				lowest_note++;
			} else {
				highest_note--;
				lowest_note--;
			}
			int i, j;
			for (i = 0; i < COLUMNS; i++) {
				for (j = 0; j < column_lengths[i]; j++) {
					if (transpose_up) {
						messages[i][j].note++;
					} else {
						messages[i][j].note--;
					}
				}
			}
		}
		all_notes_off();
	}

	if (!(btns & 2) && (new_btns & 2)) {											// Play/Pause pushed down
		if (play) {
			play = 0;
			T2CON &= ~0x8000;		// Timer off
			display_string(3, "Paused");
			display_update();
			all_notes_off();
		} else {
			play = 1;
			display_string(3, "Playing");
			display_update();
			T2CON |= 0x8000;		// Timer on
		}
	}

	if (!(btns & 8) && (new_btns & 8)) {											// Clear pushed down
		clear_column_array(column_lengths);
		all_notes_off();
		undo_index = 0;
		highest_note = 0;
		lowest_note = 127;
		display_string(0, "Saved:");
		display_int_indented(0, undo_index);
		display_update();
	}

	if (!(btns & 4) && (new_btns & 4)) {		// Undo pushed down
		if (!notes_recorded() && undo_index > 0) {
			undo_index--;
		}
		int i, j;
		for (i = 0; i < COLUMNS; i++) {
			column_lengths[i] = prev_column_lengths[undo_index][i];
		}
		all_notes_off();
		highest_note = 0;
		lowest_note = 127;
		for (i = 0; i < COLUMNS; i++) {
			for (j = 0; j < column_lengths[i]; j++) {
				unsigned char note = messages[i][j].note;
				if (note > highest_note) {
					highest_note = note;
				}
				if (note < lowest_note) {
					lowest_note = note;
				}
			}
		}
		display_string(0, "Saved:");
		display_int_indented(0, undo_index);
		display_update();
	}

	if (record && !new_record) {															// Record switch flipped down
		int different = notes_recorded();
		// If so store current column_lengths on prev_column_lengths[undo_index]
		if (different) {
			if (++undo_index == UNDO_LENGTH) { // Make sure undo_index don't get out of bounds
				undo_index = UNDO_LENGTH - 1;
			}
			int i;
			for (i = 0; i < COLUMNS; i++) {
				prev_column_lengths[undo_index][i] = column_lengths[i];
			}
		}
		display_string(2, "");
		display_string(0, "Saved:");
		display_int_indented(0, undo_index);
		display_update();
	}

	if (!record && new_record) {
		display_string(2, "Recording");
		display_update();
	}

	btns = new_btns;
	record = new_record;
}

// Reads the potentiometer and adjusts the tempo accordingly
void update_tempo() {
	/* Start sampling potentiometer, wait until conversion is done */
	AD1CON1 |= (0x1 << 1);
	while(!(AD1CON1 & (0x1 << 1)));
	while(!(AD1CON1 & 0x1));

	/* Get the analog value and update beat_length */
	unsigned int value = (ADC1BUF0 >> 5);
	beat_length = 32 - value;

	PORTE = 1 << (7 - current_column % 8); // Flash the current tempo on the LEDs

	display_string(1, "Tempo:");
	display_int_indented(1, (33 - beat_length));
	display_update();
}

int main(void) {
	quicksleep(10000000);
	init();
	clear_column_array(column_lengths);
	clear_column_array(prev_column_lengths[0]);

	// Initialise display message
	display_string(0, "Saved:");
	display_int_indented(0, 0);
	display_update();


	T2CON |= 0x8000;		// Timer on
	display_string(3, "Playing");
	display_update();

	for (;;) {

		if (time_counter > beat_length) {
			time_counter = 0;

			/* Increment the current column and wrap around at end of matrix */
			if (++current_column == COLUMNS) {
				current_column = 0;
			}

			/* If switch 4 if up play metronome */
			if (current_column % 4 == 0) {
				if (get_sw() & (1 << 3)) {
					metronome();
				}
			}

			/* For loop to go trough all rows in current column */
			int i;
			for (i = 0; i < column_lengths[current_column]; i++) {
				struct message msg = messages[current_column][i];
				if (msg.enable) {
					send_midi_message(msg);
				} else {
					messages[current_column][i].enable = 1;
				}
			}

			fix_previous_column();
		}
		handle_input();

		if (tempo_timer > 5) {
			tempo_timer = 0;
			update_tempo();
		}

	}

	return 0;
}
