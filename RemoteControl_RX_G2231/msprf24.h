#include <stdint.h>
#include "nrf24l01.h"

/* Status variable updated every time SPI I/O is performed */
extern uint8_t rf_status;
extern uint8_t rf_addr_width;
extern uint8_t rf_channel;
extern uint8_t rf_crc;
extern uint8_t rf_feature;
extern uint8_t rf_irq;
extern uint8_t rf_speed_power;

/* RF speed settings -- nRF24L01+ compliant, older nRF24L01 does not have 2Mbps. */
#define RF24_SPEED_250KBPS  0x20
#define RF24_SPEED_1MBPS    0x00
#define RF24_SPEED_2MBPS    0x08
#define RF24_SPEED_MAX      RF24_SPEED_2MBPS
#define RF24_SPEED_MIN      RF24_SPEED_250KBPS
#define RF24_SPEED_MASK     0x28

/* RF transmit power settings */
#define RF24_POWER_7DBM        0x07
    // ^ 7dBm available with SI24R1 Taiwanese knockoff modules
#define RF24_POWER_0DBM        0x06
#define RF24_POWER_MINUS6DBM   0x04
#define RF24_POWER_MINUS12DBM  0x02
#define RF24_POWER_MINUS18DBM  0x00
#define RF24_POWER_MAX         RF24_POWER_0DBM
#define RF24_POWER_MIN         RF24_POWER_MINUS18DBM
#define RF24_POWER_MASK        0x07

/* Available states for the transceiver's state machine */
#define RF24_STATE_NOTPRESENT  0x00
#define RF24_STATE_POWERDOWN   0x01
#define RF24_STATE_STANDBY_I   0x02
#define RF24_STATE_STANDBY_II  0x03
#define RF24_STATE_PTX         0x04
#define RF24_STATE_PRX         0x05
#define RF24_STATE_TEST        0x06

/* IRQ "reasons" that can be tested. */
#define RF24_IRQ_TXFAILED      0x10
#define RF24_IRQ_TX            0x20
#define RF24_IRQ_RX            0x40
#define RF24_IRQ_MASK          0x70
// Bit 7 used to signify that the app should check IRQ status, without
// wasting time in the interrupt vector trying to do so itself.
#define RF24_IRQ_FLAGGED       0x80

/* Queue FIFO states that can be tested. */
#define RF24_QUEUE_TXFULL      RF24_FIFO_FULL
#define RF24_QUEUE_TXEMPTY     RF24_TX_EMPTY
#define RF24_QUEUE_RXFULL      RF24_RX_FULL
#define RF24_QUEUE_RXEMPTY     RF24_RX_EMPTY

uint8_t r_reg(uint8_t);
void w_reg(uint8_t, uint8_t);
void w_tx_addr(uint8_t*);
void w_rx_addr(uint8_t, uint8_t*);
void w_tx_payload(uint8_t, uint8_t*);
uint8_t r_rx_payload(uint8_t, uint8_t*);
uint8_t r_rx_peek_payload_size();  // Peek size of incoming RX payload
void flush_tx();
void flush_rx();
void tx_reuse_lastpayload();   /* Enable retransmitting contents of TX FIFO endlessly until flush_tx() or the FIFO contents are replaced.
				* Actual retransmits don't occur until CE pin is strobed using pulse_ce();
				*/
void pulse_ce();  // Pulse CE pin to activate retransmission of TX FIFO contents after tx_reuse_lastpayload();
void msprf24_init();
void msprf24_close_pipe(uint8_t pipeid);       // Disable specified RX pipe
void msprf24_close_pipe_all();                       // Disable all RX pipes (used during initialization)
void msprf24_open_pipe(uint8_t pipeid, uint8_t autoack); // Enable specified RX pipe, optionally turn auto-ack (Enhanced ShockBurst) on
uint8_t msprf24_pipe_isopen(uint8_t pipeid); // Check if specified RX pipe is active
void msprf24_set_pipe_packetsize(uint8_t pipe, uint8_t size);  // Set static length of pipe's RX payloads (1-32), size=0 enables DynPD.
void msprf24_set_retransmit_delay(uint16_t us);           // 500-4000uS range, clamped by RF speed
void msprf24_set_retransmit_count(uint8_t count);       // 0-15 retransmits before MAX_RT (RF24_IRQ_TXFAILED) IRQ raised
uint8_t msprf24_get_last_retransmits();        // # times a packet was retransmitted during last TX attempt
uint8_t msprf24_get_lostpackets();      /* # of packets lost since last time the Channel was set.
	                                       * Running msprf24_set_channel() without modifying rf_channel will reset this counter.
                                               */
uint8_t msprf24_is_alive();                    // Hello World, test if chip is present and/or SPI is working.
uint8_t msprf24_set_config(uint8_t cfgval);
void msprf24_set_speed_power();                      // Commit RF speed & TX power from rf_speed_power variable.
void msprf24_set_channel();                          // Commit RF channel setting from rf_channel variable.
void msprf24_set_address_width();                    // Commit Enhanced ShockBurst Address Width from rf_addr_width variable.
void msprf24_enable_feature(uint8_t feature);    /* Enable specified feature (RF24_EN_* from nRF24L01.h, except RF24_EN_CRC) */
void msprf24_disable_feature(uint8_t feature);   /* Disable specified feature                                                */

// Change chip state and activate I/O
uint8_t msprf24_current_state();    // Get current state of the nRF24L01+ chip, test with RF24_STATE_* #define's
void msprf24_powerdown();                 // Enter Power-Down mode (0.9uA power draw)
void msprf24_standby();                   // Enter Standby-I mode (26uA power draw)
void msprf24_activate_rx();               // Enable PRX mode (~12-14mA power draw)
void msprf24_activate_tx();               // Enable Standby-II or PTX mode; TX FIFO contents will be sent over the air (~320uA STBY2, 7-11mA PTX)
uint8_t msprf24_queue_state();      // Read FIFO_STATUS register; user should compare return value with RF24_QUEUE_* #define's
uint8_t msprf24_scan();             // Scan current channel for RPD (looks for any signals > -64dBm)

// IRQ handling
uint8_t msprf24_rx_pending();		   /* Query STATUS register to determine if RX FIFO data is available for reading. */
uint8_t msprf24_get_irq_reason();            /* Query STATUS register for the IRQ flags, test with RF24_IRQ_* #define's
						    * Result is stored in rf_irq (note- RF24_IRQ_FLAGGED is not automatically cleared by this
						    * function, that's the user's responsibility.)
						    */
void msprf24_irq_clear(uint8_t irqflag);     /* Clear specified Interrupt Flags (RF24_IRQ_* #define's) from the transceiver.
		 				    * Required to allow further transmissions to continue.
						    */
uint8_t _msprf24_crc_mask();
uint8_t _msprf24_irq_mask();