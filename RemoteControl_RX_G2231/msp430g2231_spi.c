#include "msp430g2231_spi.h"

void spi_init()
{
	USICTL0 |= USISWRST;
	USICTL1 = USICKPH;
	USICKCTL = USISSEL_2 | USIDIV_0;
	USICTL0 = USIPE7 | USIPE6 | USIPE5 | USIMST | USIOE;
	USISR = 0x0000;
}

uint8_t spi_transfer(uint8_t inb)
{
	USISR = (uint16_t)inb;
	USICNT = 8;
	//__delay_cycles(500000);
	while (!(USICTL1 & USIIFG));
	return (uint8_t)USISR;
}

uint16_t spi_transfer16(uint16_t inw)
{
	USISR = inw;
	USICNT = 16 | USI16B;
	//__delay_cycles(500000);
	while (!(USICTL1 & USIIFG));
	return USISR;
}