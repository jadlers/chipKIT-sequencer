#include <pic32mx.h>

int a = 0;
int b = 0;

/* Interrupt Service Routine */
void user_isr( void ) {
	if (IFS(0) & (1 << 27)) {
		unsigned char tmp1 = U1RXREG & 0xFF;
		unsigned char tmp2 = U1RXREG & 0xFF;
		unsigned char tmp3 = U1RXREG & 0xFF;
		PORTE = tmp2;
		unsigned char arr[2];
		arr[0] = tmp2;
		arr[1] = '\0';
		display_string(1, itoaconv(a));
		display_string(2, arr);
		a++;
		IFSCLR(0) = 1 << 27;
	}
	display_string(3, itoaconv(b));
	display_update();
	b++;
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

	display_string(1,"BAJS");
	display_update();

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

int main(void) {
	unsigned char tmp;
	delay(10000000);
	ODCE = 0;
	TRISECLR = 0xFF;

	init();




	for (;;) {
		// while(!(U1STA & 0x1)); //wait for read buffer to have a value
		// tmp = U1RXREG & 0xFF;
		// while(U1STA & (1 << 9)); //make sure the write buffer is not full
		// U1TXREG = tmp;
		// PORTE = tmp;
	}

	return 0;
}
