/* Settings for 1MHz MCLK. */
#define DELAY_CYCLES_5MS       5000
#define DELAY_CYCLES_130US     130
#define DELAY_CYCLES_15US      15

 
 /* Operational pins -- IRQ, CE, CSN (SPI chip-select)
 */

/* IRQ */
#define nrfIRQport 2
#define nrfIRQpin BIT6

/* CSN SPI chip-select */
#define nrfCSNport 1
#define nrfCSNportout P1OUT
#define nrfCSNpin BIT4

/* CE Chip-Enable (used to put RF transceiver on-air for RX or TX) */
#define nrfCEport 1
#define nrfCEportout P1OUT
#define nrfCEpin BIT3