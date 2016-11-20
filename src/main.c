#include <pic32mx.h>

int a = 0;
int array_pos = 0;

struct message {
	unsigned char command;
	unsigned char note;
	unsigned char velocity;
};

struct message messages[10];

/* Interrupt Service Routine */
void user_isr( void ) {
	if (IFS(0) & (1 << 27)) {
		unsigned char cmd = U1RXREG & 0xFF;
		unsigned char note = U1RXREG & 0xFF;
		unsigned char vel = U1RXREG & 0xFF;
		struct message inst = {
			cmd,
			note,
			vel
		};

		messages[a] = inst;
		PORTE = note;
		display_string(0, itoaconv(a));
		if(++a == 10) {
			a = 0;
		}
		unsigned char arr[2];

		display_update();

		IFSCLR(0) = 1 << 27;
	}
}

void delay(int cyc) {
	int i;
	for(i = cyc; i > 0; i--);
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

	display_init();

	display_string(1,"I'm ready");
	display_string(2,"try me!");
	display_update();

	// Set buttons and switches to input
	TRISDSET = (0x7f << 5);

	/* Configure UART1 for 115200 baud, no interrupts */
	U1BRG = calculate_baudrate_divider(80000000, 31250, 0);
	U1STA = 0;
	/* 8-bit data, no parity, 1 stop bit */
	U1MODE = 0x8000;
	/* Enable transmit and recieve */
	U1STASET = 0x1400;
	PORTECLR = 0xFF;

	//Interrupt configuration
	IECSET(0) = 1 << 27; //Enable recieve interrupt (U1RXIE set)
	IPCSET(6) = 0x1f; //Set highest priority and subpriority
	IFSCLR(0) = 1 << 27; //Clear flag
	U1STASET = 1 << 7;

	enable_interrupt(); //Enable interrupts globally
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


int main(void) {
	unsigned char tmp;
	delay(10000000);
	ODCE = 0;
	TRISECLR = 0xFF;

	init();


	for (;;) {
		int pressed = get_btns();
		if (pressed & 1) {
			display_string(1,"Enter for");
			display_update();
			struct message i = messages[array_pos];

			while(U1STA & (1 << 9)); //make sure the write buffer is not full
			U1TXREG = i.command;	// command
			U1TXREG = i.note;	// note
			U1TXREG = i.velocity;	// velocity

			if (++array_pos == 10) {
				array_pos = 0;
			}
			delay(6000000);
			display_string(1,"Exit for");
			display_update();
		}
	}

	return 0;
}
