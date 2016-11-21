#include <pic32mx.h>

int array_pos_write = 0;
int array_pos_read = 0;

/* struct for MIDI messages */
struct message {
	unsigned char command;
	unsigned char note;
	unsigned char velocity;
};

struct message messages[10];	// array with 10 MIDI messages for testing

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

		if(++array_pos_write == 10) {
			array_pos_write = 0;
		}

		IFSCLR(0) = 1 << 27;	// Clear interrupt flag
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

void init() {
	/* On Uno32, we're assuming we're running with sysclk == 80 MHz */
	/* Periphial bust can run at a maximum of 40 MHz, setting PBDIV to 1 divides sysclk with 2 */
  /* OSCCONbits.PBDIV = 1; */
  OSCCONCLR = 0x100000; /* clear PBDIV bit 1 */
	OSCCONSET = 0x080000; /* set PBDIV bit 0 */

	/* Output pins for display signals */
	PORTF = 0xFFFF;
	PORTG = (1 << 9);
	ODCF = 0x0;
	ODCG = 0x0;
	TRISFCLR = 0x70;
	TRISGCLR = 0x200;

	/* Set up SPI as master */
	SPI2CON = 0;
	SPI2BRG = 4;
	/* SPI2STAT bit SPIROV = 0; */
	SPI2STATCLR = 0x40;
	/* SPI2CON bit CKP = 1; */
  SPI2CONSET = 0x40;
	/* SPI2CON bit MSTEN = 1; */
	SPI2CONSET = 0x20;
	/* SPI2CON bit ON = 1; */
	SPI2CONSET = 0x8000;

	/* Set buttons and switches to input */
	TRISDSET = (0x7f << 5);

	/* TODO Found in is1200-examples hello-serial repo, read about it */
	ODCE = 0;
	TRISECLR = 0xFF;
	/* Configure UART1 for 115200 baud, no interrupts */
	U1BRG = calculate_baudrate_divider(80000000, 31250, 0);
	U1STA = 0;
	/* 8-bit data, no parity, 1 stop bit */
	U1MODE = 0x8000;
	/* Enable transmit and recieve */
	U1STASET = 0x1480;		// Set bit 12, 10 & 7
	U1STACLR = 0x40;			// Clear bit 6
	PORTECLR = 0xFF;			// Clear LEDs

	/* Interrupt configuration */
	IECSET(0) = 1 << 27; 	// Enable recieve interrupt (U1RXIE set)
	IPCSET(6) = 0x1f; 		// Set highest priority and subpriority
	IFSCLR(0) = 1 << 27; 	// Clear flag

	enable_interrupt(); 	// Enable interrupts globally

	display_init();
	display_string(1,"I'm ready");
	display_string(2,"try me!");
	display_update();
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
		int pressed = get_btns();
		if (pressed & 1) {
			struct message msg = messages[array_pos_read];

			/* Send MIDI message */
			while(U1STA & (1 << 9));	// Make sure the write buffer is not full
			U1TXREG = msg.command;
			U1TXREG = msg.note;
			U1TXREG = msg.velocity;

			if (++array_pos_read == 10) {
				array_pos_read = 0;
			}

			display_midi_info(msg); // Display info about the send message
			quicksleep(6000000); // Delay to avoid stepping through multiple messages
		}
	}

	return 0;
}
