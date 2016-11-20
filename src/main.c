#include <pic32mx.h>

/* Interrupt Service Routine */
void user_isr( void ) {

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
	OSCCON &= ~0x180000;
	OSCCON |= 0x080000;
}

int main(void) {
	unsigned char tmp;
	delay(10000000);
	ODCE = 0;
	TRISECLR = 0xFF;

	init();

	/* Configure UART1 for 115200 baud, no interrupts */
	U1BRG = calculate_baudrate_divider(80000000, 31250, 0);
	U1STA = 0;
	/* 8-bit data, no parity, 1 stop bit */
	U1MODE = 0x8000;
	/* Enable transmit and recieve */
	U1STASET = 0x1400;
	PORTECLR = 0xFF;


	for (;;) {
		while(!(U1STA & 0x1)); //wait for read buffer to have a value
		tmp = U1RXREG & 0xFF;
		while(U1STA & (1 << 9)); //make sure the write buffer is not full
		U1TXREG = tmp;
		PORTE = tmp;

	}

	return 0;
}
