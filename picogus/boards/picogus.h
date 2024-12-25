#pragma once

// setting first overrides the value in the default header
#define PICO_FLASH_SPI_CLKDIV 4

#define PSRAM_PIN_CS 1
#define PSRAM_PIN_SCK 2
#define PSRAM_PIN_MOSI 3
#define PSRAM_PIN_MISO 0
#define PSRAM_PIO 1

#define AUDIO_DIN 16
#define AUDIO_BCK 17
#define AUDIO_LRCK 18
#define AUDIO_PIO 1
#define AUDIO_DMA_IRQN 0

#define CYW43_PIO_CLOCK_DIV 3

// pick up the rest of the settings
#include "boards/pico_w.h"
