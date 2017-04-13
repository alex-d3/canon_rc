
#include "io430.h"
#include "msprf24/msprf24.h"
//#include <stdint.h>

// PORT 1
#define LED_PIN        BIT0
#define BUTTON_AF      BIT1
#define BUTTON_AFS     BIT2

// PORT 2
#define BUTTON_LOCK    BIT0

uint8_t addr[5];    // Address of the reciever on the camera
uint8_t buf[1];     // Transmitter buffer

void main( void )
{
	rf_addr_width = 5;
	rf_speed_power = RF24_SPEED_250KBPS | RF24_POWER_0DBM;
	rf_channel = 15;
	rf_crc = RF24_EN_CRC | RF24_CRCO; // CRC enabled, 16-bit;
	
    // Stop watchdog timer to prevent time out reset
    WDTCTL = WDTPW + WDTHOLD;
    
    // Set 1 MHz clock for the lowest power consumption
    DCOCTL = CALDCO_1MHZ;
    BCSCTL1 = CALBC1_1MHZ;
	//BCSCTL2 = DIVS_0;
    BCSCTL2 = SELM_0 | DIVM_0 | DIVS_0;
	//BCSCTL3 |= LFXT1S1;
	BCSCTL3 |= LFXT1S_2;    //
	
	// Configure timer to prevent contact bounce
	TA0CTL = TACLR;	// Reset timer
	TA0CTL |= TASSEL_1 | ID_0 | MC_0;     // Compare mode, closck source: ACLK
    TA0CCR0 = 60;           // Turn off interruptions for 60 cycles
	TA0CCTL0 |= CCIE;       // Enable interruptions
	
	// Buttons setup
	P1SEL &= ~(BUTTON_AFS | BUTTON_AF);         // I/O function selected
	P1SEL2 &= ~(BUTTON_AFS | BUTTON_AF);        //
	P1DIR &= ~(BUTTON_AFS | BUTTON_AF);         // Switch to input direction
	P1OUT |= (BUTTON_AFS | BUTTON_AF);          // Pins are pulled up
	P1REN |= (BUTTON_AFS | BUTTON_AF);          // Enable pullup resitor
	P1IES |= (BUTTON_AFS | BUTTON_AF);          // Set interruption flag with a high-to-low
	                        // transition
	P1IFG &= ~(BUTTON_AFS | BUTTON_AF);         // Clear interruption flags
	P1IE |= (BUTTON_AFS | BUTTON_AF);           // Enable interruptions
	
	// Camera button lock setup
	P2SEL &= ~BUTTON_LOCK;         // I/O function selected
	P2SEL2 &= ~BUTTON_LOCK;        //
	P2DIR &= ~BUTTON_LOCK;         // Switch to input direction
	P2OUT |= BUTTON_LOCK;          // Pins are pulled up
	P2REN |= BUTTON_LOCK;          // Enable pullup resitor
	
	// LED indicator setup
	P1DIR |= LED_PIN;          // Switch to output direction
	P1OUT &= ~LED_PIN;         // Low level on output
	
	// Switch P2.6 to I/O mode
	P2SEL &= ~(BIT6 | BIT7);
	P2SEL2 &= ~(BIT6 | BIT7);
	
	// Setup nRF24L01+ module
	msprf24_init();
	msprf24_set_pipe_packetsize(0, 1);
	msprf24_open_pipe(0, 1);
	msprf24_standby();
	
	addr[0] = 0xDE; addr[1] = 0xAD; addr[2] = 0xBE; addr[3] = 0xEF; addr[4] = 0x00;
	
	w_tx_addr(addr);        // Load address of the receiver
	w_rx_addr(0, addr);     // Pipe#0 receives auto-ack's from receiver on the
	                        // camera
	
	buf[0] = 0;             // Clear the buffer
	
    P1OUT |= LED_PIN;
    __delay_cycles(100000);
	P1OUT &= ~LED_PIN;
	
	while (1)
	{
		// Wait for button click in low power mode
		__bis_SR_register(LPM3_bits);
		
		w_tx_payload(1, buf); // This runs after the interruption request is handled
		msprf24_activate_tx();
		__delay_cycles(50000); // Wait 50 ms
		
		if (rf_irq & RF24_IRQ_FLAGGED)
		{
			rf_irq &= ~RF24_IRQ_FLAGGED;
			msprf24_get_irq_reason();

			if (rf_irq & RF24_IRQ_TXFAILED)
			{
				for (uint8_t i = 0; i < 3; i++)
				{
					P1OUT |= LED_PIN;
					__delay_cycles(100000);	// Delay 100 ms
					P1OUT &= ~LED_PIN;
					__delay_cycles(100000);
				}
			}
			msprf24_irq_clear(rf_irq);
		}
	}
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void TA0_IRQ (void)
{
	TA0CCTL0 &= ~CCIFG;	// Disable IRG flag
	TA0CTL &= ~MC_1;		// Stop timer
	TA0R = 0;			// Reset timer to zero
	P1IFG &= ~(BUTTON_AFS | BUTTON_AF);		// Reset 1.1, 1.2 IRQ flags
	P1IE |= (BUTTON_AFS | BUTTON_AF);		// Enable interrupts for 1.1, 1.2
	
	
}

#pragma vector = PORT1_VECTOR
__interrupt void P1_IRQ (void)
{
	//Disable interruptions for buttons
	P1IE &= ~(BUTTON_AFS | BUTTON_AF);
	
	if (P2IN & BUTTON_LOCK)    // If button lock is not used
		TA0CCR0 = 60;
	else
		TA0CCR0 = 3000;
	
	TA0CTL |= MC_1;    // Start timer. It turns on interrupts at the end.
	
	if (P1IFG & BUTTON_AF)	// Autofocus (send 0x01 or 0x00 command)
	{
		// Exit from LPM
		__bic_SR_register_on_exit(LPM3_bits);
		
		buf[0] = ((~buf[0]) & BIT0) | (buf[0] & 0xFE);
		if (P2IN & BUTTON_LOCK)
			P1IES = ((~P1IES) & BUTTON_AF) | (P1IES & ~BUTTON_AF);
		
		/*
		if (P1IES & BIT1)
		{
			buf[0] |= BIT0;
			P1IES &= ~BIT1;
		}
		else
		{
			buf[0] &= ~BIT0;
			P1IES |= BIT1;
		}
		*/
		P1IFG &= ~BUTTON_AF;    // Clear interruption flag
	}
	else if (P1IFG & BUTTON_AFS)	// AF + Shutter
	{
		__bic_SR_register_on_exit(LPM3_bits);
		
		buf[0] = ((~buf[0]) & BIT1) | (buf[0] & 0xFD);
		if (P2IN & BUTTON_LOCK)
			P1IES = ((~P1IES) & BUTTON_AFS) | (P1IES & ~BUTTON_AFS);
		/*
		if (P1IES & BIT2)
		{
			buf[0] |= BIT1;
			P1IES &= ~BIT2;
		}
		else
		{
			buf[0] &= ~BIT1;
			P1IES |= BIT2;
		}
		*/
		P1IFG &= ~BUTTON_AFS;    // Clear interruption flag
	}
}