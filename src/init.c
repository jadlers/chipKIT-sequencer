#include <pic32mx.h>

void shield_input_init() {
  /* Set buttons and switches to input */
	TRISDSET = (0x7f << 5);

  /* Potentiometer */
  /* PORTB.2 is analog pin with potentiometer*/
	AD1PCFG = ~(1 << 2);
	TRISBSET = (1 << 2);
	/* Use pin 2 for positive */
	AD1CHS = (0x2 << 16);

	/* Data format in uint32, 0 - 1024
	Manual sampling, auto conversion when sampling is done
	FORM = 0x4; SSRC = 0x7; CLRASAM = 0x0; ASAM = 0x0; */
	AD1CON1 = (0x4 << 8) | (0x7 << 5);
	AD1CON2 = 0x0;
	AD1CON3 |= (0x1 << 15);

	/* Turn on ADC */
	AD1CON1 |= (0x1 << 15);
}

void timer_init() {
  /* Timer setup */
	T2CON = 0;
	T2CON |= 0x0060;		// Prescale 1:256 (T2CON bit 6-4 = 110)
	PR2 = 12500;				// Set period to 12500
	TMR2 = 0;						// Clear Timer2 counter

  /* Interrupt configuration */
	IECSET(0) = 1 << 8;	// Enable interrupts for Timer2
	IPCSET(2) = 0xE;		// Set prio = 3 & subprio = 2
	IFSCLR(0) = 1 << 8;	// Clear interupt flag
}

void led_init() {
  /* LED init */
	ODCE = 0;
	TRISECLR = 0xFF;   // Set LED pins as output
  PORTECLR = 0xFF;   // Clear LEDs
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

void uart_init() {
  /* On Uno32, we're assuming we're running with sysclk == 80 MHz */
	/* Periphial bust can run at a maximum of 40 MHz, setting PBDIV to 1 divides sysclk with 2 */
  /* OSCCONbits.PBDIV = 1; */
  OSCCONCLR = 0x100000; // Clear PBDIV bit 1
	OSCCONSET = 0x080000; // Set PBDIV bit 0

  /* Configure UART1 */
  U1BRG = calculate_baudrate_divider(80000000, 31250, 0); // 31250 baudrate
  U1STA = 0;
  U1MODE = 0x8000; // 8-bit data, no parity, 1 stop bit

  /* Enable transmit and recieve */
  U1STASET = 0x1480;		// Set bit 12, 10 & 7
  U1STACLR = 0x40;			// Clear bit 6

  /* Interrupt configuration */
  IECSET(0) = 1 << 27; 	// Enable recieve interrupt (U1RXIE set)
  IPCSET(6) = 0x1f; 		// Set highest priority and subpriority
  IFSCLR(0) = 1 << 27; 	// Clear flag
}

void init() {
  uart_init();
  led_init();
  timer_init();
  display_init();
  shield_input_init();
	enable_interrupt(); // Enable interrupts globally

  /* Display that the initialization is complete */
	display_string(1,"I'm ready");
	display_string(2,"try me!");
	display_update();
}
