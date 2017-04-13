
#include "io430.h"
#include "msprf24.h"

uint8_t rf_status = 0;
uint8_t rf_addr_width = 5;
uint8_t rf_speed_power = RF24_SPEED_250KBPS | RF24_POWER_0DBM;
uint8_t rf_channel = 15;
uint8_t rf_crc = RF24_EN_CRC | RF24_CRCO; // CRC enabled, 16-bit
uint8_t rf_feature;
uint8_t rf_irq;

int main( void )
{
	uint8_t addr[5];
	uint8_t buf[1];
	buf[0] = 0;
	
	// Stop watchdog timer to prevent time out reset
	WDTCTL = WDTPW + WDTHOLD;
	DCOCTL = CALDCO_1MHZ;
	BCSCTL1 = CALBC1_1MHZ;
	BCSCTL2 = DIVS_0;
	
	P1DIR |= 0x07;
	P1OUT &= ~0x07;
	
	msprf24_init();
	
	msprf24_set_pipe_packetsize(0, 1);
	msprf24_open_pipe(0, 1);
	msprf24_standby();
	
	addr[0] = 0xDE; addr[1] = 0xAD; addr[2] = 0xBE; addr[3] = 0xEF; addr[4] = 0x00;
	w_rx_addr(0, addr);  // Pipe#0 receives auto-ack's
	
	if (!(RF24_QUEUE_RXEMPTY & msprf24_queue_state()))
	{
		flush_rx();
	}
	msprf24_activate_rx();
	//__bis_SR_register(LPM4_bits);
	
	while (1)
	{
		__bis_SR_register(LPM4_bits);
		if (rf_irq & RF24_IRQ_FLAGGED)
		{
			rf_irq &= ~RF24_IRQ_FLAGGED;
			msprf24_get_irq_reason();
		}
		if (rf_irq & RF24_IRQ_RX)
		{
			do
			{
				r_rx_payload(1, buf);
				
				if (buf[0] & BIT0)
				{
					P1OUT |= BIT1;
				}
				else if (buf[0] & BIT1)
				{
					P1OUT |= BIT2;
				}
				else
				{
					P1OUT &= ~0x06;
				}
				/*
				P1OUT |= 0x01;
					__delay_cycles(100000);
					P1OUT &= ~0x01;
					__delay_cycles(100000);
				*/
				/*
				for (short i = 0; i < 8; i++)
				{
					P1OUT |= (buf[0] >> i) & 0x01;
					__delay_cycles(100000);
					P1OUT &= ~0x01;
					__delay_cycles(100000);
				}
				*/
			}
			while (msprf24_rx_pending());
			msprf24_irq_clear(RF24_IRQ_RX);
		}
		//__bis_SR_register(LPM4_bits);
	}
}
