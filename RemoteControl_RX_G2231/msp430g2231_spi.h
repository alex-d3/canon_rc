#include <cstdint>
#include "io430G2231.h"

void spi_init();
uint8_t spi_transfer(uint8_t); // SPI xfer 1 byte
uint16_t spi_transfer16(uint16_t);  // SPI xfer 2 bytes